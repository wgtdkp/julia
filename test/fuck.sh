#! /bin/bash

export GATEWAY_INTERFACE="CGI/1.1"
export SCRIPT_FILENAME="/home/wgtdkp/julia/www/index.php"
export REQUEST_METHOD="POST"
export REDIRECT_STATUS=200
#export REQUEST_METHOD="GET"

#export SERVER_PROTOCOL="HTTP/1.1"
export REMOTE_HOST="127.0.0.1"
export CONTENT_LENGTH=13
#export QUERY_STRING="user=kangping"
#export HTTP_ACCEPT="text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
export CONTENT_TYPE="application/x-www-form-urlencoded"
export BODY="user=kangping"

exec echo "$BODY" | /usr/bin/php5-cgi
#exec /usr/bin/php5-cgi