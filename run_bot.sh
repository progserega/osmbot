#!/bin/bash
api_server="http://api06.dev.openstreetmap.org"
login="`cat login.conf`"

# Скачиваем блок:
curl -u "${login}" -o in.osm -X GET "${api_server}/api/0.6/map?bbox=131.9416676,43.1104802,131.944927,43.1128104"
# правим и сохраняем в out.osm
./osmpatch

# генерируем файл изменений:
/opt/osmosis/bin/osmosis --read-xml out.osm --read-xml in.osm --derive-change --write-xml-change diff.osm

# Сама загрузка:
# Создаём changeset:
curl -u "${login}" -o changeset.id -d @templates/changeset.template -X PUT "${api_server}/api/0.6/changeset/create"
changeset_id="`cat changeset.id`"

# Меняем changeset-ы в diff-файле:
cat diff.osm|sed "s/changeset=\"[0-9]*\"/changeset=\"${changeset_id}\"/g" > diff.tmp
mv diff.tmp diff.osm

# Загружаем изменения:
curl -u "${login}" -d @diff.osm -X POST "${api_server}/api/0.6/changeset/${changeset_id}/upload"
# Закрываем changeset:
curl -u "${login}" -X PUT "${api_server}/api/0.6/changeset/${changeset_id}/close"
