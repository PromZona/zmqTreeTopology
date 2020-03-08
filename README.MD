# Requirments:
## Libs
* libsodium.dll
* libzmq.dll + libzmq.lib
## Compile
First of all compile server
> g++ server.cpp -L./ -**libzmq** 

Then compile client
> g++ client.cpp -L./ -**libzmq** 

Start by server.exe

# Commands:
* create [**pid**] - creating new node with **pid**
* remove [**pid**] - deleting node by **pid**
* exec [**pid**] [**string**] [**int**] - create new note in dict. **string** is key, **int** is value
* heartbit [**int**] - start process of heartbitting every **int** milliseconds
* status - print root node status

## Bugs:
* [ ] exec - неправильная последовательность аргументов, либо другой тип переменных выдаёт непредсказуемое поведение
* [X] В случае убийства отца - его дети продолжают жить
* [ ] В случае убийства нода - сервер продолжает думать, что он существует и не даёт создать нод с таким же именем
* [X] При команде remove для удаления root'a - сервер вылетает
* [X] При убийстве root'a - сервер не хочет создавать нового
* [X] При обращении к удаленному ноду командой exec - вылетает сервер

## Features:
* [ ] Перевести Сервер на обработку сообщений через manager
