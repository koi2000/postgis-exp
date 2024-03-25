SELECT b.id,ST_3DDistance(a.geom, b.geom) as mindis ,ST_3DMaxDistance(a.geom, b.geom) as maxdis FROM nuclei_20 a, nuclei_20 b WHERE a.id <> b.id AND a.id = '1';

CREATE INDEX vessel_box_geom_idx ON "public"."vessel_box" USING RTREE (geom);
CREATE INDEX vessel_100_geom_idx ON "public"."vessel_100" USING RTREE (geom);
CREATE INDEX vessel_80_geom_idx ON "public"."vessel_80" USING RTREE (geom);
CREATE INDEX vessel_60_geom_idx ON "public"."vessel_60" USING RTREE (geom);
CREATE INDEX vessel_40_geom_idx ON "public"."vessel_40" USING RTREE (geom);
CREATE INDEX vessel_20_geom_idx ON "public"."vessel_20" USING RTREE (geom);

CREATE INDEX nuclei_box_geom_idx ON "public"."nuclei_box" USING RTREE (geom);
CREATE INDEX nuclei_100_geom_idx ON "public"."nuclei_100" USING RTREE (geom);
CREATE INDEX nuclei_80_geom_idx ON "public"."nuclei_80" USING RTREE (geom);
CREATE INDEX nuclei_60_geom_idx ON "public"."nuclei_60" USING RTREE (geom);
CREATE INDEX nuclei_40_geom_idx ON "public"."nuclei_40" USING RTREE (geom);
CREATE INDEX nuclei_20_geom_idx ON "public"."nuclei_20" USING RTREE (geom);