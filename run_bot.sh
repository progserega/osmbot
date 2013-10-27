#!/bin/bash
bbox_primorye="129.4189453,42.2285174,139.1748047,48.7199612"
bbox_school14="131.9416676,43.1104802,131.944927,43.1128104"
bbox_test="130.0,43.0,131.00001,44.01"

bbox_to_process="${bbox_primorye}"
#bbox_to_process="${bbox_test}"
#bbox_to_process="${bbox_school14}"

# Обрабатываем квадраты по 0.5 градуса
step="0.5"
api_server="http://api06.dev.openstreetmap.org"
login="`cat login.conf`"



process_bbox()
{
	# parameters - bbox: $1,$2,$3,$4
	echo "============================================"
	echo "Start processing bbox: ${1},${2} - ${3},${4}"

	# Скачиваем блок:
	echo "Dounloading bbox from API:"
	curl -u "${login}" -o in.osm -X GET "${api_server}/api/0.6/map?bbox=${1},${2},${3},${4}"

	# правим и сохраняем в out.osm
	echo "Start parsing:"
	./osmpatch 2>/dev/null

	# генерируем файл изменений:
	echo "Generate diff-file by osmosis:"
	/opt/osmosis/bin/osmosis --read-xml out.osm --read-xml in.osm --derive-change --write-xml-change diff.osm

	# Сама загрузка:
	# Создаём changeset:
	echo "Create changeset:"
	curl -u "${login}" -o changeset.id -d @templates/changeset.template -X PUT "${api_server}/api/0.6/changeset/create"
	changeset_id="`cat changeset.id`"
	echo "Changeset id=`cat changeset.id`"


	# Меняем changeset-ы в diff-файле:
	echo "Change changeset id's in diff.osm:"
	cat diff.osm|sed "s/changeset=\"[0-9]*\"/changeset=\"${changeset_id}\"/g" > diff.tmp
	mv diff.tmp diff.osm

	# Загружаем изменения:
	echo "Send diff.osm to API-server:"
	curl -u "${login}" -d @diff.osm -X POST "${api_server}/api/0.6/changeset/${changeset_id}/upload"
	# Закрываем changeset:
	echo "Close changeset id=`cat changeset.id`:"
	curl -u "${login}" -X PUT "${api_server}/api/0.6/changeset/${changeset_id}/close"

	echo "End processing bbox ${1},${2} - ${3},${4}"
	echo "============================================"
}



x1="`echo ${bbox_to_process}|awk '{print $1}' FS=','`"
y1="`echo ${bbox_to_process}|awk '{print $2}' FS=','`"
x2="`echo ${bbox_to_process}|awk '{print $3}' FS=','`"
y2="`echo ${bbox_to_process}|awk '{print $4}' FS=','`"

bbox_x1="${x1}"
bbox_y1="${y1}"
bbox_x2="${x2}"
bbox_y2="${y2}"

bbox_y1="${y1}"
bbox_y2="${y1}"
while /bin/true
do
	bbox_y2="`echo ${bbox_y1}+${step}|bc -l`"
	if [ $(echo "$bbox_y2 > $y2" | bc) -eq 1 ]
	then
		bbox_y2="${y2}"
	fi
	# processing:

		bbox_x1="${x1}"
		bbox_x2="${x1}"
		while /bin/true
		do
			bbox_x2="`echo ${bbox_x1}+${step}|bc -l`"
			if [ $(echo "$bbox_x2 > $x2" | bc) -eq 1 ]
			then
				bbox_x2="${x2}"
			fi

			#############################
			# processing:
			process_bbox "${bbox_x1}" "${bbox_y1}" "${bbox_x2}" "${bbox_y2}"

			#############################
			bbox_x1="${bbox_x2}"
			if [ $(echo "$bbox_x1 >= $x2" | bc) -eq 1 ]
			then
				break
			fi
		done
	bbox_y1="${bbox_y2}"
	if [ $(echo "$bbox_y1 >= $y2" | bc) -eq 1 ]
	then
		break
	fi
done

echo "Success processing bbox: ${bbox_to_process} by rules.xml"

exit 0



