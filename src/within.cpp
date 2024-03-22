#include "config.h"
#include <iostream>
#include <libpq-fe.h>
#include <map>
#include <pqxx/pqxx>

/**
 * nn wthin 50
 * 首先根据mbb来筛，如果mbb的最小距离大于50，就不用再管了，直接筛掉
 * 然后根据lod1，根据ha
 * 两个高lod物体间的距离
 * dis(a,b) >= dis(a',b') - h(a,a') - h(b,b')
 * dis(a,b) <= dis(a',b') + h(a,a') + h(b,b')
 */
int distance = 50;
int target = 3;

class Range {
  public:
    int id;
    float mindis;
    float maxdis;
    Range(){};
    Range(int id_, float mindis_, float maxdis_) : id(id_), mindis(mindis_), maxdis(maxdis_) {}
    Range(float mindis_, float maxdis_) : mindis(mindis_), maxdis(maxdis_) {}
};

std::string buildstr = "hostaddr=" + host + " dbname=" + dbname + " user=" + user + " password=" + password;

std::string buildQueryMbbSql(int id) {
    char sql[512];
    sprintf(sql,
            "SELECT b.id,ST_3DDistance(a.geom, b.geom) as mindis ,ST_3DMaxDistance(a.geom, b.geom) as maxdis FROM "
            "nuclei_box a, nuclei_box b WHERE a.id <> b.id AND a.id = '%d';",
            id);
    return std::string(sql);
}

std::string buildIdList(const std::vector<int>& ids) {
    std::string idList;
    for (size_t i = 0; i < ids.size(); ++i) {
        idList += std::to_string(ids[i]);
        if (i < ids.size() - 1) {
            idList += ",";
        }
    }
    return idList;
}

/**
 * 返回的是当前lod下的distance和hausdorff距离
 */
std::string buildQueryLodSql(int lod, int id, std::vector<int> ids) {
    char sql[512];
    sprintf(sql,
            "SELECT b.id, ST_3DDistance(a.geom, b.geom) as dis, "
            "b.hausdorff, b.phausdorff FROM "
            "nuclei_%d a, nuclei_%d b WHERE a.id <> b.id AND a.id = '%d' AND b.id IN (%s);",
            lod, lod, id, buildIdList(ids).c_str());
    return std::string(sql);
}

std::string buildQueryHausdorffSql(int lod, int id) {
    char sql[512];
    sprintf(sql,
            "SELECT a.hausdorff, a.phausdorff "
            "FROM nuclei_%d a "
            "WHERE a.id = '%d' ",
            lod, id);
    return std::string(sql);
}

// 根据query mbb返回的结果进行更新
void parseDistanceResult(pqxx::result& rows, std::map<int, Range>& candidates) {
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            if (candidates.count(rows[i]["id"].as<int>())) {
                candidates[rows[i]["id"].as<int>()].mindis =
                    std::max(candidates[rows[i]["id"].as<int>()].mindis, rows[i]["mindis"].as<float>());
                candidates[rows[i]["id"].as<int>()].maxdis =
                    std::min(candidates[rows[i]["id"].as<int>()].maxdis, rows[i]["maxdis"].as<float>());
            } else {
                candidates[rows[i]["id"].as<int>()] =
                    Range(rows[i]["mindis"].as<float>(), rows[i]["maxdis"].as<float>());
            }
        }
    }
}

// 根据query lod返回的结果进行更新
void parseLodDistanceResult(pqxx::result& rows, std::map<int, Range>& candidates, std::pair<float, float> item) {
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            candidates[rows[i]["id"].as<int>()].mindis =
                rows[i]["dis"].as<float>() - rows[i]["phausdorff"].as<float>() - item.second;
            candidates[rows[i]["id"].as<int>()].maxdis =
                rows[i]["dis"].as<float>() + rows[i]["hausdorff"].as<float>() + item.first;
        }
    }
}

/**
 * 过滤掉不符合条件的distance，符合条件的直接假如，待确定的留在其中
 */
void filterByDistance(std::map<int, Range>& ranges, std::vector<int>& result, int distance) {
    for (auto it = ranges.begin(); it != ranges.end();) {
        if ((*it).second.maxdis <= distance) {
            result.push_back((*it).first);
            it = ranges.erase(it);
        } else if ((*it).second.mindis > distance) {
            it = ranges.erase(it);
        } else {
            it++;
        }
    }
}

std::vector<int> mapKeysToVector(const std::map<int, Range>& myMap) {
    std::vector<int> keys;
    for (const auto& pair : myMap) {
        keys.push_back(pair.first);
    }
    return keys;
}

int main() {
    pqxx::connection c(buildstr);
    pqxx::work w(c);
    pqxx::result rows = w.exec(buildQueryMbbSql(target));

    std::map<int, Range> candidates;
    parseDistanceResult(rows, candidates);
    std::vector<int> result;
    filterByDistance(candidates, result, distance);
    // 当候选集维空的时候，说明已经确定了结果
    if (candidates.empty()) {
        exit(0);
    }
    for (int lod = 20; lod <= 100; lod += 20) {
        rows = w.exec(buildQueryHausdorffSql(lod, target));
        std::pair<float, float> targetHausdorff =
            std::make_pair(rows[0]["hausdorff"].as<float>(), rows[0]["phausdorff"].as<float>());
        rows = w.exec(buildQueryLodSql(lod, target, mapKeysToVector(candidates)));
        parseLodDistanceResult(rows, candidates, targetHausdorff);
        filterByDistance(candidates, result, distance);
        if (candidates.empty()) {
            for (int i = 0; i < result.size(); i++) {
                std::cout << result[i] << std::endl;
            }
            exit(0);
        }
    }
    return 0;
}
