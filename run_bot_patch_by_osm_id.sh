#!/bin/bash

if [ -z "$1" ]
then
	echo "Need 1 argument: config file"
	echo "Example use:"
	echo "  $0 osmbot.conf"
	echo ""
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
	echo "api_server: ${api_server}"
	echo "log file: ${log}"
	echo "============================="
fi

tmp_file="`mktemp /tmp/osmbot_tmp_file_XXXX`"
osm_in_file="`mktemp /tmp/osmbot_in.osm_XXXX`"
osm_out_file="`mktemp /tmp/osmbot_out.osm_XXXXX`"
osm_diff_file="`mktemp /tmp/osmbot_diff.osm_XXXX`"
osm_changeset_template_file="`mktemp /tmp/osmbot_changeset_template_file_XXXXXX`"
error_file="`mktemp /tmp/osmbot_error_file_XXXXXX`"

process_id()
{
	# parameters - osm_type $1, osm_id $2
	echo "============================================" >> "${log}"
	echo "Start processing: ${1}#${2}" >> "${log}"

	# Скачиваем блок:
	echo "Downloading osm element from API:" >> "${log}"
	echo "start command: curl $curl_opt -u \"${login}:${passwd}\" -o \"${osm_in_file}\" -X GET \"${api_server}/api/0.6/${1}/$2\"" >> "${log}"
  curl $curl_opt -u "${login}:${passwd}" -o "${osm_in_file}" -X GET "${api_server}/api/0.6/${1}/$2" &> "${error_file}"
  curl_return_status="$?"
	if [ -z "`cat ${osm_in_file}|grep '<osm'|grep 'version='|grep 'generator='`" -o ! 0 -eq "${curl_return_status}" ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute curl GET!"
		echo "`date +%Y.%m.%d-%T`: error execute curl GET!" >> "${log}"
		echo "server ansver:
`cat ${osm_in_file}`" 
		echo "server ansver:
`cat ${osm_in_file}`" >> "${log}"
		echo "last 50 lines error-log:" >> "${log}"
		cat "${error_file}"|tail -n 50 >> "${log}"
		return 1
	fi


	# правим и сохраняем в out.osm
  rules_file_path="${osm_id_rules_dir}/rule_${1}#${2}.xml"
	echo "Start parsing by command:" >> "${log}"
	echo "${osmpatch} -r ${rules_file_path} -i ${osm_in_file} -o ${osm_out_file} > ${tmp_file} 2>${error_file}" >> "${log}"
	"${osmpatch}" -r "${rules_file_path}" -i "${osm_in_file}" -o "${osm_out_file}" > "${tmp_file}" 2>"${error_file}"
	if [ ! 0 -eq $? ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute osmpatch!"
		echo "`date +%Y.%m.%d-%T`: error execute osmpatch!" >> "${log}"
		echo "last 50 lines error-log:" >> "${log}"
		cat "${error_file}"|tail -n 50 >> "${log}"
		return 1
	fi
	# Выводим список изменённых элементов:
	echo "`date +%Y.%m.%d-%T`: Success patched elements:" >> "${log}"
	cat "${tmp_file}"|grep "Start patch this element"|sed "s/.*element_url=//"|while read url
	do
		echo "${api_server}/${url}" >> "${log}"
	done
	

	# генерируем файл изменений:
	echo "Generate diff-file by osmosis:" >> "${log}"
	"${osmosis}" --read-xml "${osm_out_file}" --read-xml "${osm_in_file}" --derive-change --write-xml-change "${osm_diff_file}" &> "${error_file}"
	if [ ! 0 -eq $? ]
	then
		echo "`date +%Y.%m.%d-%T`: error execute osmosis!" 
		echo "`date +%Y.%m.%d-%T`: error execute osmosis!"  >> "${log}"
		echo "last 50 lines error-log:" >> "${log}"
		cat "${error_file}"|tail -n 50 >> "${log}"
		return 1
	fi


	# Сама загрузка:
	
	# Меняем changeset-ы в diff-файле:
	echo "Change changeset id's in ${osm_diff_file} to changeset='${changeset_id}':" >> "${log}"
	cat "${osm_diff_file}"|sed "s/changeset=\"[0-9]*\"/changeset=\"${changeset_id}\"/g" > "${tmp_file}"
	cat "${tmp_file}" > "${osm_diff_file}" 

	# Загружаем изменения:
	echo "Send ${osm_diff_file} to API-server:" >> "${log}"
	echo "start command: curl $curl_opt -u \"${login}:${passwd}\" -d @\"${osm_diff_file}\" -X POST \"${api_server}/api/0.6/changeset/${changeset_id}/upload\"" >> "${log}"
  if [ $DEBUG != "yes" ]
  then
    curl $curl_opt -u "${login}:${passwd}" -d @"${osm_diff_file}" -X POST "${api_server}/api/0.6/changeset/${changeset_id}/upload" &> "${error_file}"
    curl_return_status="$?"
    if [ -z "`cat ${osm_diff_file}|grep '<diffResult\|<osmChange'|grep 'version='|grep 'generator='`" -o ! 0 -eq "${curl_return_status}" ]
    then
      echo "`date +%Y.%m.%d-%T`: error execute curl upload diff!" 
      echo "`date +%Y.%m.%d-%T`: error execute curl upload diff!" >> "${log}"
      echo "server ansver:
  `cat ${osm_diff_file}`" 
      echo "server ansver:
  `cat ${osm_diff_file}`" >> "${log}"
      echo "last 50 lines error-log:" >> "${log}"
      cat "${error_file}"|tail -n 50 >> "${log}"
      return 1
    fi
  else
    curl_return_status=0
  fi

	echo "End processing: ${1}#${2}" >> "${log}"
	echo "============================================" >> "${log}"
}



# =================== Начало скрипта ====================
echo "================ Start processing osm element: ${1}#${2} by rules.xml =============="
echo " " >> "${log}"
echo " " >> "${log}"
echo "`date +%Y.%m.%d-%T`: ================ Start processing osm element: ${1}#${2} by rules.xml ==============" >> "${log}"
# В начале создаём единый пачсет для всех квадратов, чтобы было проще в случае необходимости откатывать изменения:
# Создаём changeset:
echo "Create changeset:" >> "${log}"
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
	echo "==================================================================================" >> "${log}"
	echo "||      Create changeset with comment=\"${changeset_comment_tag}\"   ||" >> "${log}"
	echo "==================================================================================" >> "${log}"
fi
echo "</changeset>
</osm>" >> "${osm_changeset_template_file}"
####################
cat /dev/null> "${tmp_file}"
echo "start command: curl $curl_opt -u \"${login}:${passwd}\" -o \"${tmp_file}\" -d @\"${osm_changeset_template_file}\" -X PUT \"${api_server}/api/0.6/changeset/create\"" >> "${log}"
if [ $DEBUG != "yes" ]
then
  curl $curl_opt -u "${login}:${passwd}" -o "${tmp_file}" -d @"${osm_changeset_template_file}" -X PUT "${api_server}/api/0.6/changeset/create" &> "${error_file}"
  curl_return_status="$?"
  if [ ! 1 -eq "`cat ${tmp_file}|egrep '^[0-9]+$'|wc -l`" -o ! 0 -eq "`cat ${tmp_file}|egrep -v '^[0-9]+$'|wc -l`" -o ! 0 -eq "${curl_return_status}" ]
  then
      echo "`date +%Y.%m.%d-%T`: error execute curl create changeset!" 
      echo "`date +%Y.%m.%d-%T`: error execute curl create changeset!" >> "${log}"
      echo "server ansver:
      `cat ${tmp_file}`" 
      echo "server ansver:
      `cat ${tmp_file}`" >> "${log}"
      echo "last 50 lines error-log:" >> "${log}"
      cat "${error_file}"|tail -n 50 >> "${log}"
      exit 1
  fi
  changeset_id="`cat ${tmp_file}`"
else
  curl_return_status="0"
  changeset_id="1"
fi
echo "Changeset id=${changeset_id}" >> "${log}"
echo "Created changeset id=${changeset_id}"

exit_status=0

num_eterations=`cat $osm_id_csv|wc -l`

success_procent_by_iteration=`echo "100/$num_eterations"|bc -l`
echo "Будет проведено $num_eterations обработок объектов."
procent_success="0"

while read text
do
  osm_type="`echo $text|awk '{print $1}' FS='#'`"
  osm_id="`echo $text|awk '{print $2}' FS='#'`"
  #############################
  # processing:
  process_id "${osm_type}" "${osm_id}"
  if [ ! 0 -eq $? ]
  then
    echo "`date +%Y.%m.%d-%T`: error process_id()!" 
    echo "`date +%Y.%m.%d-%T`: error process_id()!" >> "${log}"
    exit_status=1
    break
  fi

  # show procent of success:
  procent_success=`echo "${procent_success} + ${success_procent_by_iteration}"|bc -l`
  echo "Success: `echo ${procent_success}|sed 's/^\./0./'|sed 's/\..*//'`%..."

  #############################
done < ${osm_id_csv}

# Закрываем единый для всех квадратов changeset:
echo "`date +%Y.%m.%d-%T`: Close changeset id=${changeset_id}:" >> "${log}"
echo "`date +%Y.%m.%d-%T`: Close changeset id=${changeset_id}:"
echo "start command: curl $curl_opt -u \"${login}:${passwd}\" -X PUT -d '' \"${api_server}/api/0.6/changeset/${changeset_id}/close\"" >> "${log}" 
if [ $DEBUG != "yes" ]
then
  curl $curl_opt -u "${login}:${passwd}" -X PUT -d '' "${api_server}/api/0.6/changeset/${changeset_id}/close" 2>"${tmp_file}" 1>"${error_file}"
  curl_return_status="$?"

  if [ ! 0 -eq "`cat ${error_file}|wc -l`" -o ! 0 -eq "${curl_return_status}" ]
  then
      echo "`date +%Y.%m.%d-%T`: error execute curl close changeset!" 
      echo "`date +%Y.%m.%d-%T`: error execute curl close changeset!" >> "${log}"
      echo "server ansver:
      `cat ${error_file}`" 
      echo "server ansver:
      `cat ${error_file}`" >> "${log}"
      echo "last 50 lines stdout:" >> "${log}"
      cat "${tmp_file}"|tail -n 50 >> "${log}"
      exit 1
  fi
else
  curl_return_status="0"
fi

if [ 0 -eq "${exit_status}" ]
then
		echo "Success processing bbox: ${bbox_to_process} by rules.xml" >> "${log}"
		echo "`date +%Y.%m.%d-%T`: Changeset url: ${api_server}/browse/changeset/${changeset_id}" >> "${log}"
		echo "`date +%Y.%m.%d-%T`: ================ Success processing bbox: ${bbox_to_process} by rules.xml ==============" >> "${log}"
		echo "`date +%Y.%m.%d-%T`: Changeset url: ${api_server}/browse/changeset/${changeset_id}"
		echo "================ Success processing bbox: ${bbox_to_process} by rules.xml =============="
else
		echo "ERROR processing bbox: ${bbox_to_process} by rules.xml" >> "${log}"
		echo "`date +%Y.%m.%d-%T`: ================ ERROR processing bbox: ${bbox_to_process} by rules.xml ==============" >> "${log}"
		echo "================ ERROR processing bbox: ${bbox_to_process} by rules.xml =============="
fi

if [ $DEBUG != "yes" ]
then
  rm "${tmp_file}"
  rm "${osm_in_file}"
  rm "${osm_out_file}"
  rm "${osm_diff_file}"
  rm "${osm_changeset_template_file}"
  rm "${error_file}"
fi

exit ${exit_status}

