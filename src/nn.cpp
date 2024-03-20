#include "config.h"
#include <iostream>
#include <libpq-fe.h>

std::string buildstr = "hostaddr=" + host + " dbname=" + dbname + " user=" + user + " password=" + password;

/**
 * 如何实现NN查询
 * 首先得到mbb之间的距离，筛选一波
 * 得到lod 1之间的距离 筛选一波
 * 然后decode
*/

int main() {
    PGconn* conn = PQconnectdb(buildstr.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return 1;
    }
    PGresult* res =
        PQexec(conn, "SELECT ST_AsText(ST_Envelope(geom)) AS bounding_box FROM public.nuclei_20 WHERE id = 3;");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        PQfinish(conn);
        return 1;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        std::cout << "Bounding Box: " << PQgetvalue(res, i, 0) << std::endl;
    }
    PQclear(res);
    PQfinish(conn);
    return 0;
}
