#!/bin/bash

# Список тестов
tests=(
  "test-1a test-1b result-1"
  "test-2a test-2b result-2"
  "test-3a test-3b result-3"
  "test-4a test-4b result-4"
  "test-5a test-5b result-5"
)

echo -e "\nRunning tests\n"
echo "------------------------------------------"
# Цикл по тестам
for test in "${tests[@]}"; do
  # Разделяем строку на переменные (например, test-1a, test-1b, result-1)
  set -- $test
  input1="./tests/$1"
  input2="./tests/$2"
  output="./tests/$3"

  # Выполнение программы
  ./a.out "$input1" "$input2" "$output"

  # Проверка, если тест не прошел
  if [ $? -eq 0 ]; then
    echo "Test passed"
  else
    echo "Test failed for $input1 and $input2. Please check the output."
  fi

  echo "------------------------------------------"
done
