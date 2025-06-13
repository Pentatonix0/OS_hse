#!/bin/bash

SERVER_IP="127.0.0.1"
SERVER_PORT="8888"

echo "Компиляция сервера и клиента..."
gcc server.c -o server -pthread
gcc client.c -o client

echo "Запуск сервера на $SERVER_IP:$SERVER_PORT..."
./server $SERVER_PORT &

sleep 1 # Дать серверу время запуститься

echo "Запуск двух садовников..."

./client $SERVER_IP $SERVER_PORT 1 &
./client $SERVER_IP $SERVER_PORT 2 &

echo "Все садовники запущены"
wait
echo "Все садовники завершили работу"