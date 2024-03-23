#include "config.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <libpq-fe.h>
#include <map>
#include <pqxx/pqxx>
#include <set>
#include <unistd.h>
#include <vector>
std::string buildstr = "hostaddr=" + host + " dbname=" + dbname + " user=" + user + " password=" + password;

/**
 * 如何实现NN查询
 * 先根据mbb找到前100个，然后从最低的lod开始找，期间要涉及到排序，如果
 */
int target = 3;
int number = 3;
std::string table1 = "nuclei";
std::string table2 = "nuclei";
int distance = 300;

class Range {
  public:
    int id;
    float mindis;
    float maxdis;
    Range(){};
    Range(int id_) : id(id_){};
    Range(int id_, float mindis_, float maxdis_) : id(id_), mindis(mindis_), maxdis(maxdis_) {}
    Range(float mindis_, float maxdis_) : mindis(mindis_), maxdis(maxdis_) {}
};

/**
 * 找出来前100最小距离最小的
 */
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
std::string buildQueryLodSql(int lod, int id, std::vector<int> ids) {
    char sql[2048];
    sprintf(sql,
            "SELECT b.id, ST_3DDistance(a.geom, b.geom) as dis, "
            "b.hausdorff, b.phausdorff FROM "
            "%s_%d a, %s_%d b WHERE a.id <> b.id AND a.id = '%d' AND b.id IN (%s);",
            table1.c_str(), lod, table2.c_str(), lod, id, buildIdList(ids).c_str());
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
void parseDistanceResult(pqxx::result& rows, std::map<int, Range>& candidates) {
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            candidates[rows[i]["id"].as<int>()] = Range(rows[i]["id"].as<int>());
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
    // filterByDistance(candidates, number);
    // 当候选集维空的时候，说明已经确定了结果
    if (candidates.size() == number) {
        exit(0);
    }

    auto afterTime = std::chrono::steady_clock::now();
    double duration_millsecond = std::chrono::duration<double, std::milli>(afterTime - beforeTime).count();
    std::cout << target << " " << duration_millsecond << " ";

    for (int lod = 20; lod <= 100; lod += 20) {
        rows = w.exec(buildQueryHausdorffSql(lod, target));
        std::pair<float, float> targetHausdorff =
            std::make_pair(rows[0]["hausdorff"].as<float>(), rows[0]["phausdorff"].as<float>());
        rows = w.exec(buildQueryLodSql(lod, target, mapKeysToVector(candidates)));
        parseLodDistanceResult(rows, candidates, targetHausdorff);
        filterByDistance(candidates, number);
        if (candidates.size() == number) {
            // for (auto item : candidates) {
            //     std::cout << item.first << std::endl;
            // }
            break;
        }
    }
    auto afterTime2 = std::chrono::steady_clock::now();
    duration_millsecond = std::chrono::duration<double, std::milli>(afterTime2 - afterTime).count();
    std::cout << duration_millsecond << std::endl;
    return 0;
}
