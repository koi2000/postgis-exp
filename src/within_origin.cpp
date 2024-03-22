#include "config.h"
#include <chrono>
#include <iostream>
#include <libpq-fe.h>
#include <map>
#include <pqxx/pqxx>
#include <unistd.h>

/**
 * nn wthin 50
 * 首先根据mbb来筛，如果mbb的最小距离大于50，就不用再管了，直接筛掉
 * 然后根据lod1，根据ha
 * 两个高lod物体间的距离
 * dis(a,b) >= dis(a',b') - h(a,a') - h(b,b')
 * dis(a,b) <= dis(a',b') + h(a,a') + h(b,b')
 */
int distance = 50;
int target = 1;
std::string table1 = "nuclei";
std::string table2 = "nuclei";

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
            "SELECT b.id as id FROM %s_box a, %s_box b WHERE a.id <> b.id AND a.id = %d AND ST_3DDWithin(a.geom, "
            "b.geom, %d);",
            table1.c_str(), table2.c_str(), id, distance);
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
            "%s_100 a, %s_100 b WHERE a.id <> b.id AND a.id = '%d' AND b.id IN (%s);",
            table1.c_str(), table2.c_str(), id, buildIdList(ids).c_str());
    return std::string(sql);
}

std::string buildQueryHausdorffSql(int lod, int id) {
    char sql[512];
    sprintf(sql,
            "SELECT a.hausdorff, a.phausdorff "
            "FROM %s_%d a "
            "WHERE a.id = '%d' ",
            table1.c_str(), lod, id);
    return std::string(sql);
}

// 根据query mbb返回的结果进行更新
void parseOriginResult(pqxx::result& rows, std::map<int, Range>& candidates) {
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            candidates[rows[i]["id"].as<int>()].mindis = rows[i]["dis"].as<float>();
            candidates[rows[i]["id"].as<int>()].maxdis = rows[i]["dis"].as<float>();
        }
    }
}

// 根据query mbb返回的结果进行更新
void parseDistanceResult(pqxx::result& rows, std::map<int, Range>& candidates) {
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            candidates[rows[i]["id"].as<int>()] = Range();
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

int main(int argc, char** argv) {
    int opt;
    while ((opt = getopt(argc, argv, "t:1:2:")) != -1) {
        switch (opt) {
            case 't': target = std::stoi(optarg); break;
            case '1': table1 = optarg; break;
            case '2': table2 = optarg; break;
            default: std::cerr << "Usage: " << argv[0] << " -t <target> -1 <table1> -2 <table2>" << std::endl; return 1;
        }
    }

    pqxx::connection c(buildstr);
    pqxx::work w(c);

    auto beforeTime = std::chrono::steady_clock::now();
    pqxx::result rows = w.exec(buildQueryMbbSql(target));
    std::map<int, Range> candidates;
    parseDistanceResult(rows, candidates);
    std::vector<int> result;
    // filterByDistance(candidates, result, distance);
    // 当候选集维空的时候，说明已经确定了结果
    if (candidates.empty()) {
        exit(0);
    }
    auto afterTime = std::chrono::steady_clock::now();
    double duration_millsecond = std::chrono::duration<double, std::milli>(afterTime - beforeTime).count();
    std::cout << target << " " << duration_millsecond << " ";

    rows = w.exec(buildQueryOriginSql(target, mapKeysToVector(candidates)));
    parseOriginResult(rows, candidates);
    filterByDistance(candidates, result, distance);
    // if (candidates.empty()) {
    //     for (int i = 0; i < result.size(); i++) {
    //         std::cout << result[i] << std::endl;
    //     }
    // }
    auto afterTime2 = std::chrono::steady_clock::now();
    duration_millsecond = std::chrono::duration<double, std::milli>(afterTime2 - afterTime).count();
    std::cout << duration_millsecond << std::endl;
    return 0;
}