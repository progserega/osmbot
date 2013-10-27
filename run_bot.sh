#!/bin/bash
bbox_primorye="129.4189453,42.2285174,139.1748047,48.7199612"
bbox_school14="131.9416676,43.1104802,131.944927,43.1128104"
bbox_test="130.0,43.0,131.00001,44.01"

bbox_to_process="${bbox_primorye}"
#bbox_to_process="${bbox_test}"
bbox_to_process="${bbox_school14}"

# Обрабатываем квадраты по 0.5 градуса
step="0.5"
api_server="http://api06.dev.openstreetmap.org"
login="`cat login.conf`"

log="osmbot.log"



process_bbox()
{
	# parameters - bbox: $1,$2,$3,$4
	echo "============================================" >> "${log}"
	echo "Start processing bbox: ${1},${2} - ${3},${4}" >> "${log}"

	# Скачиваем блок:
	echo "Dounloading bbox from API:" >> "${log}"
	curl -u "${login}" -o in.osm -X GET "${api_server}/api/0.6/map?bbox=${1},${2},${3},${4}" >> "${log}"

	# правим и сохраняем в out.osm
	echo "Start parsing:" >> "${log}"
	./osmpatch 2>/dev/null >> "${log}"

	# генерируем файл изменений:
	echo "Generate diff-file by osmosis:" >> "${log}"
	/opt/osmosis/bin/osmosis --read-xml out.osm --read-xml in.osm --derive-change --write-xml-change diff.osm >> "${log}"

	# Сама загрузка:
	changeset_id="`cat changeset.id`"

	# Меняем changeset-ы в diff-файле:
	echo "Change changeset id's in diff.osm to changeset='${changeset_id}':" >> "${log}"
	cat diff.osm|sed "s/changeset=\"[0-9]*\"/changeset=\"${changeset_id}\"/g" > diff.tmp >> "${log}"
	mv diff.tmp diff.osm >> "${log}"

	# Загружаем изменения:
	echo "Send diff.osm to API-server:" >> "${log}"
	curl -u "${login}" -d @diff.osm -X POST "${api_server}/api/0.6/changeset/${changeset_id}/upload" >> "${log}"

	echo "End processing bbox ${1},${2} - ${3},${4}" >> "${log}"
	echo "============================================" >> "${log}"
}



# =================== Начало скрипта ====================

echo "================ Start processing bbox: ${bbox_to_process} by rules.xml =============="
echo "`date +%Y.%m.%d-%T`: ================ Start processing bbox: ${bbox_to_process} by rules.xml ==============" >> "${log}"
# В начале создаём единый пачсет для всех квадратов, чтобы было проще в случае необходимости откатывать изменения:
# Создаём changeset:
echo "Create changeset:" >> "${log}"
cat /dev/null> changeset.id
curl -u "${login}" -o changeset.id -d @templates/changeset.template -X PUT "${api_server}.ru/api/0.6/changeset/create" &>> "${log}"
echo "curl_return=$?"

changeset_id="`cat changeset.id`"
echo "Changeset id=${changeset_id}" >> "${log}"


# get coordinates from bbox:
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

# просчитываем количество итераций:
num_x_iterac=`echo "($x2-$x1)/${step}"|bc -l`
celoe="`echo ${num_x_iterac}|sed 's/\..*//'`"
if [ -z ${celoe} ]
then
	celoe="0"
fi

if [ $(echo "$num_x_iterac > $celoe" | bc) -eq 1 ]
then
	num_x_iterac="`echo $celoe + 1|bc`"
fi

num_y_iterac=`echo "($y2-$y1)/${step}"|bc -l`
celoe="`echo ${num_y_iterac}|sed 's/\..*//'`"
if [ -z ${celoe} ]
then
	celoe="0"
fi

if [ $(echo "$num_y_iterac > $celoe" | bc) -eq 1 ]
then
	num_y_iterac="`echo $celoe + 1|bc`"
fi

num_eterations=`echo "$num_x_iterac * $num_y_iterac"|bc -l`

success_procent_by_iteration=`echo "100/$num_eterations"|bc -l`
echo "Будет проведено $num_eterations обработок квадратов по ${step}x${step} градусов."
procent_success="0"

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
			#process_bbox "${bbox_x1}" "${bbox_y1}" "${bbox_x2}" "${bbox_y2}"

			# show procent of success:
			procent_success=`echo "${procent_success} + ${success_procent_by_iteration}"|bc -l`
			echo "Success: `echo ${procent_success}|sed 's/^\./0./'|sed 's/\..*//'`%..."

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

# Закрываем единый для всех квадратов changeset:
echo "`date +%Y.%m.%d-%T`: Close changeset id=${changeset_id}:" >> "${log}"
curl -u "${login}" -X PUT "${api_server}/api/0.6/changeset/${changeset_id}/close" &>> "${log}"
echo "curl_return=$?"

echo "Success processing bbox: ${bbox_to_process} by rules.xml" >> "${log}"
echo "`date +%Y.%m.%d-%T`: ================ Success processing bbox: ${bbox_to_process} by rules.xml ==============" >> "${log}"


echo "================ Success processing bbox: ${bbox_to_process} by rules.xml =============="

exit 0



