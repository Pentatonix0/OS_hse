# Отчет Лебедев Андрей Андреевич БПИ234
## Вариант 35
Разработать программу, которая на основе анализа двух ASCII–
строк формирует на выходе строку, содержащую символы, присутствующие в обеих строках (пересечение символов). Каждый
символ в соответствующей выходной строке должен встречаться
только один раз. Входными и выходными параметрами являются
имена трех файлов, задающих входные и выходную строки.
## 8 баллов
### Схема решения
```
+-------------------+       +-------------------+
|   Process 1        |       |   Process 2        |
| (File Reading &    |       | (Data Processing & |
| FIFO Writing)      |       | FIFO Writing)      |
+-------------------+       +-------------------+
          |                           |
          | 1. Reads input files       |
          | (source1, source2)         |
          |                           |
          | 2. Writes data to FIFOs    |
          | (read_channel1, read_channel2) |
          |                           |
          |                           | 3. Reads data from FIFOs
          |                           | (read_channel1, read_channel2)
          |                           |
          |                           | 4. Processes data
          |                           | (Finds common symbols)
          |                           |
          |                           | 5. Writes result to FIFO
          |                           | (write_channel)
          |                           |
          | 6. Reads result from FIFO  |
          | (write_channel)            |
          |                           |
          | 7. Writes result to file   |
          | (destination)              |
+-------------------+       +-------------------+
```
### Использование
```
./process1 <input1> <input2> <result> <chan1> <chan2> <chan3> & sleep 1 && ./process2 <chan1> <chan2> <chan3> &
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
common symbols: "#()*,-.8:;=>ACEINPRSTV[]_abcdefghijklmnoprstuvwxy
```