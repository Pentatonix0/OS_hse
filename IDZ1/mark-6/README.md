# Отчет Лебедев Андрей Андреевич БПИ234
## Вариант 35
Разработать программу, которая на основе анализа двух ASCII–
строк формирует на выходе строку, содержащую символы, присутствующие в обеих строках (пересечение символов). Каждый
символ в соответствующей выходной строке должен встречаться
только один раз. Входными и выходными параметрами являются
имена трех файлов, задающих входные и выходную строки.
## 6 баллов
### Схема решения
```
+-------------------+       +-------------------+
|   Parent Process   |       |   Child Process    |
| (Main Process)     |       | (Data Processing)  |
+-------------------+       +-------------------+
          |                           |
          | 1. Reads input files       |
          | (source1, source2)         |
          |                           |
          | 2. Writes data to pipes    |
          | (read_channel1, read_channel2) |
          |                           |
          |                           | 3. Reads data from pipes
          |                           | (read_channel1, read_channel2)
          |                           |
          |                           | 4. Processes data
          |                           | (Finds common symbols)
          |                           |
          |                           | 5. Writes result to pipe
          |                           | (write_channel)
          |                           |
          | 6. Reads result from pipe  |
          | (write_channel)            |
          |                           |
          | 7. Writes result to file   |
          | (destination)              |
+-------------------+       +-------------------+
```
### Использование
```
./a.out <input1> <input2> <result> <chan1> <chan2> <chan3>
```
### Тестирование
```
chmod +x run_tests.sh
./run_tests.sh
```
### Тесты
#### test-1 (max 4 symbols)
```
common symbols:abc
```
#### test-2 (max 26 symbols)
```
common symbols: aeflmnoprt
```
#### test-3 (max 269 symbols)
```
common symbols: ,-abcdefghilmnoprstuw
```
#### test-4 (max 2082 symbols)
```
common symbols: ,ABNYabcdefghijklmnoprstuvwy
```
#### test-5 (max 21016 symbols)
```
common symbols: !"#()*+,-.8:;<=>@ACEILNPRSTV[]_abcdefghijklmnopqrstuvwxyz
```