#include "config.h"
#include <iostream>
#include <libpq-fe.h>
#include <pqxx/pqxx>

std::string buildstr = "hostaddr=" + host + " dbname=" + dbname + " user=" + user + " password=" + password;

int main() {
    pqxx::connection c(buildstr);

    pqxx::work w(c);
    pqxx::result rows =
        w.exec("SELECT ST_AsText(ST_Envelope(geom)) AS bounding_box FROM public.nuclei_20 WHERE id = 3;");
    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            std::cout << rows[i][j] << std::endl;
        }
    }
    return 0;
}
