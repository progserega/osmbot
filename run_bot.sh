#!/bin/bash

if [ -z "$1" ]
then
	echo "Need 1 argument: config file\nExample use:\n   $0 osmbot.conf\n"
	exit 1
fi

config="${1}"

source "${config}"

# Полученные настройки:
if [ -z "${login}" -o -z "${passwd}" -o -z "${bbox_to_process}" -o -z "${api_server}" -o -z "${step}" -o -z "${log}" ]
then
	echo "Please, edit ${config}. See osmbot.conf_exmple..."
	exit 1
else
	echo "============================="
	echo "Read settings from ${config}:"
	echo "login: ${login}:${passwd}"
	echo "bbox_to_process: ${bbox_to_process}"
	echo "api_server: ${api_server}"
	echo "step: ${step}"
	echo "log file: ${log}"
	echo "============================="
fi

tmp_file="`mktemp /tmp/osmbotXXXX`"
osm_in_file="`mktemp /tmp/in.osmXXXX`"
osm_out_file="`mktemp /tmp/out.osmXXXXX`"
osm_diff_file="`mktemp /tmp/diff.osmXXXX`"
osm_changeset_template_file="`mktemp /tmp/changeset_template_fileXXXXXX`"

process_bbox()
{
	# parameters - bbox: $1,$2,$3,$4
	echo "============================================" >> "${log}"
	echo "Start processing bbox: ${1},${2} - ${3},${4}" >> "${log}"

	# Скачиваем блок:
	echo "Dounloading bbox from API:" >> "${log}"
	curl -u "${login}:${passwd}" -o "${osm_in_file}" -X GET "${api_server}/api/0.6/map?bbox=${1},${2},${3},${4}" >> "${log}"
	curl_return_status="$?"
	if [ -z "`cat ${osm_in_file}|grep '<osm'|grep 'version='|grep 'generator='`" -o ! 0 -eq "${curl_return_status}" ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute curl GET!"
		echo "`date +%Y.%m.%d-%T`: error execute curl GET!" >> "${log}"
		return 1
	fi


	# правим и сохраняем в out.osm
	echo "Start parsing:" >> "${log}"
	"${osmpatch}" -r "${rules_file}" -i "${osm_in_file}" -o "${osm_out_file}" > "${tmp_file}"
	if [ ! 0 -eq $? ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute osmpatch!"
		echo "`date +%Y.%m.%d-%T`: error execute osmpatch!" >> "${log}"
		return 1
	fi
	# Выводим список изменённых элементов:
	echo "`date +%Y.%m.%d-%T`: Success patched elements:"
	cat "${tmp_file}"|grep "Start patch this element"|sed "s/.*element_url=//"|while read url
	do
		echo "${api_server}/${url}"
	done
	echo "`date +%Y.%m.%d-%T`: Success patched elements:" >> "${log}"
	cat "${tmp_file}"|grep "Start patch this element"|sed "s/.*element_url=//"|while read url
	do
		echo "${api_server}/${url}" >> "${log}"
	done
	

	# генерируем файл изменений:
	echo "Generate diff-file by osmosis:" >> "${log}"
	"${osmosis}" --read-xml "${osm_out_file}" --read-xml "${osm_in_file}" --derive-change --write-xml-change "${osm_diff_file}" >> "${log}"
	if [ ! 0 -eq $? ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute osmosis!" 
		echo "`date +%Y.%m.%d-%T`: error execute osmosis!"  >> "${log}"
		return 1
	fi


	# Сама загрузка:
	
	# Меняем changeset-ы в diff-файле:
	echo "Change changeset id's in ${osm_diff_file} to changeset='${changeset_id}':" >> "${log}"
	cat "${osm_diff_file}"|sed "s/changeset=\"[0-9]*\"/changeset=\"${changeset_id}\"/g" > "${tmp_file}"
	cat "${tmp_file}" > "${osm_diff_file}" 

	# Загружаем изменения:
	echo "Send ${osm_diff_file} to API-server:" >> "${log}"
	curl -u "${login}:${passwd}" -d @"${osm_diff_file}" -X POST "${api_server}/api/0.6/changeset/${changeset_id}/upload" > "${tmp_file}"
	curl_return_status="$?"
	if [ -z "`cat ${osm_diff_file}|grep '<diffResult'|grep 'version='|grep 'generator='`" -o ! 0 -eq "${curl_return_status}" ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute curl upload diff!" 
		echo "`date +%Y.%m.%d-%T`: error execute curl upload diff!" >> "${log}"
		return 1
	fi

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
echo "`date +%Y.%m.%d-%T`: create changeset template:"
echo "`date +%Y.%m.%d-%T`: create changeset template:" >> "${log}"

# Формируем changeset template: ########
echo "<osm>
<changeset>" > "${osm_changeset_template_file}"
if [ ! -z "${changeset_created_by_tag}" ]
then 
	echo "<tag k=\"created_by\" v=\"${changeset_created_by_tag}\"/>" >> "${osm_changeset_template_file}"
fi
if [ ! -z "${changeset_comment_tag}" ]
then 
	echo "<tag k=\"comment\" v=\"${changeset_comment_tag}\"/>" >> "${osm_changeset_template_file}"
fi
echo "</changeset>
</osm>" >> "${osm_changeset_template_file}"
####################

curl -u "${login}:${passwd}" -o changeset.id -d @"${osm_changeset_template_file}" -X PUT "${api_server}/api/0.6/changeset/create" &>> "${log}"
echo "curl_return=$?"

changeset_id="`cat changeset.id`"
echo "Changeset id=${changeset_id}" >> "${log}"
echo "Created changeset id=${changeset_id}"

exit_status=0

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
			process_bbox "${bbox_x1}" "${bbox_y1}" "${bbox_x2}" "${bbox_y2}"
			if [ ! 0 -eq $? ]
			then
				echo "`date +%Y.%m.%d-%T`: error process_bbox()!" 
				echo "`date +%Y.%m.%d-%T`: error process_bbox()!" >> "${log}"
				exit_status=1
				break
			fi

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
	if [ 1 -eq $exit_status ]
	then
		break
	fi
done

# Закрываем единый для всех квадратов changeset:
echo "`date +%Y.%m.%d-%T`: Close changeset id=${changeset_id}:" >> "${log}"
echo "`date +%Y.%m.%d-%T`: Close changeset id=${changeset_id}:"
curl -u "${login}:${passwd}" -X PUT "${api_server}/api/0.6/changeset/${changeset_id}/close" &>> "${log}"
echo "curl_return=$?"

if [ 0 -eq "${exit_status}" ]
then
		echo "Success processing bbox: ${bbox_to_process} by rules.xml" >> "${log}"
		echo "`date +%Y.%m.%d-%T`: ================ Success processing bbox: ${bbox_to_process} by rules.xml ==============" >> "${log}"
		echo "================ Success processing bbox: ${bbox_to_process} by rules.xml =============="
else
		echo "ERROR processing bbox: ${bbox_to_process} by rules.xml" >> "${log}"
		echo "`date +%Y.%m.%d-%T`: ================ ERROR processing bbox: ${bbox_to_process} by rules.xml ==============" >> "${log}"
		echo "================ ERROR processing bbox: ${bbox_to_process} by rules.xml =============="
fi

rm "${tmp_file}"
rm "${osm_in_file}"
rm "${osm_out_file}"
rm "${osm_diff_file}"
rm "${osm_changeset_template_file}"

exit ${exit_status}

