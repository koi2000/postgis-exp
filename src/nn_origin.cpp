#include "config.h"
#include <algorithm>
#include <iostream>
#include <libpq-fe.h>
#include <map>
#include <pqxx/pqxx>
#include <set>
#include <vector>
std::string buildstr = "hostaddr=" + host + " dbname=" + dbname + " user=" + user + " password=" + password;

/**
 * 如何实现NN查询
 * 先根据mbb找到前100个，然后从最低的lod开始找，期间要涉及到排序，如果
 */
int target = 3;
int number = 3;

class Range {
  public:
    int id;
    float mindis;
    float maxdis;
    Range(){};
    Range(int id_, float mindis_, float maxdis_) : id(id_), mindis(mindis_), maxdis(maxdis_) {}
    Range(float mindis_, float maxdis_) : mindis(mindis_), maxdis(maxdis_) {}
};

/**
 * 找出来前100最小距离最小的
 */
std::string buildQueryMbbSql(int id) {
    char sql[512];
    sprintf(sql,
            "SELECT b.id,ST_3DDistance(a.geom, b.geom) as mindis ,ST_3DMaxDistance(a.geom, b.geom) as maxdis FROM "
            "nuclei_box a, nuclei_box b WHERE a.id <> b.id AND a.id = '%d' order by mindis limit 100;",
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
std::string buildQueryOriginSql(int id, std::vector<int> ids) {
    char sql[512];
    sprintf(sql,
            "SELECT b.id, ST_3DDistance(a.geom, b.geom) as dis FROM "
            "nuclei_100 a, nuclei_100 b WHERE a.id <> b.id AND a.id = '%d' AND b.id IN (%s);",
            id, buildIdList(ids).c_str());
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
            candidates[rows[i]["id"].as<int>()].id = rows[i]["id"].as<int>();
        }
    }
}

// 根据query lod返回的结果进行更新
void parseLodOriginResult(pqxx::result& rows, std::map<int, Range>& candidates) {
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            candidates[rows[i]["id"].as<int>()].mindis = rows[i]["dis"].as<float>();
            candidates[rows[i]["id"].as<int>()].maxdis = rows[i]["dis"].as<float>();
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

std::vector<Range> mapValuesToVector(const std::map<int, Range>& myMap) {
    std::vector<Range> values;
    for (const auto& pair : myMap) {
        values.push_back(pair.second);
    }
    return values;
}

/**
 * 过滤掉不符合条件的distance，符合条件的直接假如，待确定的留在其中
 */
void filterByDistance(std::map<int, Range>& ranges, int number) {
    std::set<int> removable;
    std::set<float> window;
    std::vector<Range> rarr = mapValuesToVector(ranges);
    std::sort(rarr.begin(), rarr.end(), [](Range a, Range b) { return a.maxdis < b.maxdis; });
    for (int i = 0; i < number; i++) {
        window.insert(rarr[i].maxdis);
    }
    for (int i = 0; i < rarr.size(); i++) {
        if (rarr[i].mindis > *(window.rbegin())) {
            removable.insert(rarr[i].id);
        }
    }
    for (int id : removable) {
        ranges.erase(id);
    }
}

int main() {
    pqxx::connection c(buildstr);
    pqxx::work w(c);
    auto beforeTime = std::chrono::steady_clock::now();
    pqxx::result rows = w.exec(buildQueryMbbSql(target));
    std::map<int, Range> candidates;
    parseDistanceResult(rows, candidates);
    filterByDistance(candidates, number);
    rows = w.exec(buildQueryOriginSql(target, mapKeysToVector(candidates)));
    parseOriginResult(rows, candidates);
    filterByDistance(candidates, result, distance);
    if (candidates.empty()) {
        for (int i = 0; i < result.size(); i++) {
            std::cout << result[i] << std::endl;
        }
    }
    auto afterTime = std::chrono::steady_clock::now();
    double duration_millsecond = std::chrono::duration<double, std::milli>(afterTime - beforeTime).count();
	std::cout << duration_millsecond << "ms" << std::endl;
    return 0;
}
