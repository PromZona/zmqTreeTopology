#include <zmq.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <windows.h>
#include "command.h"

// TODO: дописать цикл в котором надо увеличивать время и проверять не прошло ли слишком много
// Если прошло слишком много - убить себя. Если пришел сигнал от отца - обнулить таймер.

class client
{
public:
    client(int p, int id, int fp, int fid)
    {
        left = nullptr;
        right = nullptr;
        port = p;
        pid = id;
        clientManager = new manager();

        father = new Node(fid, fp);
    }

    ~client()
    {
        if (father)
            delete father;
        if (left)
            delete left;
        if (right)
            delete right;
    }

    void Start()
    {
        clientManager->port = this->port;
        std::thread managerPr(&manager::Start, clientManager);
        std::thread proc(&this->commandProcess, this);
        std::thread childRTL(&this->sendChildrensReasonToLive, this);
        managerPr.join();
        proc.join();
        childRTL.join();
    }

    void commandProcess()
    {
        auto beatTime1 = std::chrono::system_clock::now();
        auto beatTime2 = std::chrono::system_clock::now();
        while (clientManager->status)
        {
            if (clientManager->commandsQreceived.size() > 0)
            {
                clientManager->commandQMutexR.lock();
                command cmdProc = clientManager->commandsQreceived.front();
                clientManager->commandQMutexR.unlock();
                clientManager->commandsQreceived.pop();

                switch (cmdProc.commandCode)
                {
                case 1:
                    createNewNode(cmdProc);
                    break;
                case 2:
                    deleteNode(cmdProc);
                    break;
                case 3:
                    addToDict(cmdProc);
                    break;
                case 4:
                    findInDict(cmdProc);
                    break;
                case 5:
                    beatTime1 = std::chrono::system_clock::now();
                    beatTime2 = std::chrono::system_clock::now();
                    dur = beatTime2 - beatTime1;
                    heartbitActivate(cmdProc);
                    break;
                case 6:
                    sendToServer(cmdProc);
                    break;
                case 7:
                    pingbyFather(cmdProc);
                    break;
                case -1:
                    if (cmdProc.nodeP == left)
                    {
                        delete left;
                        left = nullptr;
                    }
                    if (cmdProc.nodeP == right)
                    {
                        delete right;
                        right = nullptr;
                    }
                    break;
                default:
                    break;
                }
            }

            beatTime2 = std::chrono::system_clock::now();
            std::chrono::duration<float> buf = beatTime2 - beatTime1;
            beatTime1 = beatTime2;
            if (heartBeat)
            {
                dur += buf;
                if (dur.count() * 1000 >= static_cast<float>(heartBeatTime))
                {
                    dur = beatTime2 - beatTime1;
                    command cmd;
                    cmd.bit(father, this->pid);
                    clientManager->commandsQsend.push(cmd);
                }
            }

            this->fPing += buf;
            if (fPing.count() * 1000 >= 1000 * 4)
            {
                clientManager->status = false;
            }

            Sleep(SLEEP_TIME);
        }
    }

    // [1]
    void createNewNode(command cmdProc)
    {
        int clientPid = 0, clientPort = 0, fatherPid = 0, fatherPort = 0;
        sscanf(cmdProc.args.c_str(), "%d %d %d %d", &clientPid, &clientPort, &fatherPid, &fatherPort);

        Node *buf = nullptr;
        if (clientPid < pid) // if clientPid < PID, then must proccess left side
            buf = left;
        else if (clientPid > pid) // if clientPid < PID, then must proccess right side
            buf = right;
        else
            std::cout << "Really?\n";

        if (buf != nullptr) // if son exist, send to him message
        {
            command newcommand;
            if (clientPid < pid) // if clientPid < PID, then must proccess left side
                newcommand = command(left, clientPid, clientPort, this->pid, this->port);
            else if (clientPid > pid) // if clientPid > PID, then must proccess right side
                newcommand = command(right, clientPid, clientPort, this->pid, this->port);

            clientManager->commandQMutexS.lock();
            clientManager->commandsQsend.push(newcommand);
            clientManager->commandQMutexS.unlock();
        }
        else // if son doesnot exist, create new one
        {
            if (clientPid < pid) // if clientPid < PID, then must proccess left side
            {
                left = new Node(clientPid, clientPort);
            }
            else if (clientPid > pid) // if clientPid < PID, then must proccess right side
            {
                right = new Node(clientPid, clientPort);
            }

            std::string procname = "client.exe";
            std::string cmdArgs = procname;
            cmdArgs.append(" ");
            cmdArgs.append(std::to_string(clientPid));
            cmdArgs.append(" ");
            cmdArgs.append(std::to_string(clientPort));
            cmdArgs.append(" ");
            cmdArgs.append(std::to_string(this->pid)); // father pid
            cmdArgs.append(" ");
            cmdArgs.append(std::to_string(this->port)); // father port
            char *cmdArgschar = new char[cmdArgs.length() + 1];
            strcpy(cmdArgschar, cmdArgs.c_str());

            if (clientPid < pid) // if clientPid < PID, then must proccess left side
            {
                if (CreateProcessA(procname.c_str(), cmdArgschar, NULL, NULL,
                                   FALSE, 0, NULL, NULL, &left->strupInfo, &left->processInfo))
                {
                    std::cout << "Ok: " << clientPid << std::endl;
                }
                else
                {
                    std::cout << "Cannot create proccess - " << GetLastError() << std::endl;
                }
            }
            else if (clientPid > pid) // if clientPid > PID, then must proccess right side
            {
                if (CreateProcessA(procname.c_str(), cmdArgschar, NULL, NULL,
                                   FALSE, 0, NULL, NULL, &right->strupInfo, &right->processInfo))
                {
                    std::cout << "Ok: " << clientPid << std::endl;
                }
                else
                {
                    std::cout << "Cannot create proccess - " << GetLastError() << std::endl;
                }
            }
        }
    }

    // [2]
    void deleteNode(command cmdProc)
    {
        int clientPid = 0;
        sscanf(cmdProc.args.c_str(), "%d", &clientPid);
        if ((this->pid == clientPid))
        {
            clientManager->status = false;
        }
        else
        {
            if (clientPid < this->pid)
            {
                if (left)
                {
                    command newcommand(left, clientPid);
                    clientManager->commandQMutexS.lock();
                    clientManager->commandsQsend.push(newcommand);
                    clientManager->commandQMutexS.unlock();
                }
                else
                {
                    std::cout << "Error: Not found\n";
                }
            }
            else
            {
                if (right)
                {
                    command newcommand(right, clientPid);
                    clientManager->commandQMutexS.lock();
                    clientManager->commandsQsend.push(newcommand);
                    clientManager->commandQMutexS.unlock();
                }
                else
                {
                    std::cout << "Error: Not found\n";
                }
            }
            Sleep(SLEEP_TIME);
            if (left)
            {
                if (clientPid == left->pid)
                {
                    delete left;
                    left = nullptr;
                }
            }

            if (right)
            {
                if (clientPid == right->pid)
                {
                    delete right;
                    right = nullptr;
                }
            }
        }
    }

    // [3]
    void addToDict(command cmdProc)
    {
        int clientPid = 0, num = 0;
        char str[MAX_ARGS_SIZE];
        sscanf(cmdProc.args.c_str(), "%d %s %d", &clientPid, str, &num);
        if (clientPid != this->pid)
        {
            if (clientPid > this->pid)
            {
                if (right)
                {
                    command nwcmd(right, clientPid, str, num);
                    clientManager->commandQMutexS.lock();
                    clientManager->commandsQsend.push(nwcmd);
                    clientManager->commandQMutexS.unlock();
                }
                else
                {
                    std::cout << "Error: " << clientPid << " doesn't exist\n";
                }
            }
            else if (clientPid < this->pid)
            {
                if (left)
                {
                    command nwcmd(left, clientPid, str, num);
                    clientManager->commandQMutexS.lock();
                    clientManager->commandsQsend.push(nwcmd);
                    clientManager->commandQMutexS.unlock();
                }
                else
                {
                    std::cout << "Error: " << clientPid << " doesn't exist\n";
                }
            }
            return;
        }
        mapa.insert(std::pair<std::string, int>(str, num));
        std::cout << "Ok:" << this->pid << std::endl;
    }

    // [4]
    void findInDict(command cmdProc)
    {
        int clientPid = 0, num = 0;
        char str[MAX_ARGS_SIZE];
        sscanf(cmdProc.args.c_str(), "%d %s", &clientPid, str);
        if (clientPid != this->pid)
        {
            if (clientPid > this->pid)
            {
                if (right)
                {
                    command nwcmd(right, clientPid, str);
                    clientManager->commandQMutexS.lock();
                    clientManager->commandsQsend.push(nwcmd);
                    clientManager->commandQMutexS.unlock();
                }
                else
                {
                    std::cout << "Error: " << clientPid << " doesn't exist\n";
                }
            }
            else if (clientPid < this->pid)
            {
                if (left)
                {
                    command nwcmd(left, clientPid, str);
                    clientManager->commandQMutexS.lock();
                    clientManager->commandsQsend.push(nwcmd);
                    clientManager->commandQMutexS.unlock();
                }
                else
                {
                    std::cout << "Error: " << clientPid << " doesn't exist\n";
                }
            }
            return;
        }
        auto kk = mapa.find(str);
        if (kk != mapa.end())
        {
            num = mapa.find(str)->second;
            std::cout << "Ok:" << this->pid << " " << num << std::endl;
        }
        else
        {
            std::cout << "Ok:" << this->pid << " " << str << " not found" << std::endl;
        }
    }

    // [5]
    void heartbitActivate(command cmdProc)
    {
        sscanf(cmdProc.args.c_str(), "%d", &heartBeatTime);
        if (left != nullptr)
        {
            command ncmd;
            ncmd.heartbit(left, heartBeatTime);
            clientManager->commandQMutexS.lock();
            clientManager->commandsQsend.push(ncmd);
            clientManager->commandQMutexS.unlock();
        }

        if (right != nullptr)
        {
            command ncmd;
            ncmd.heartbit(right, heartBeatTime);
            clientManager->commandQMutexS.lock();
            clientManager->commandsQsend.push(ncmd);
            clientManager->commandQMutexS.unlock();
        }
        heartBeat = true;
    }

    // [6]
    void sendToServer(command cmdProc)
    {
        int a = 0;
        sscanf(cmdProc.args.c_str(), "%d", &a);
        command ncmd;
        ncmd.bit(father, a);
        clientManager->commandQMutexS.lock();
        clientManager->commandsQsend.push(ncmd);
        clientManager->commandQMutexS.unlock();
    }

    // [7]
    void pingbyFather(command cmdProc)
    {
        auto time = std::chrono::system_clock::now();
        fPing = time - time;
    }

    // Send for children that father still alive
    void sendChildrensReasonToLive()
    {
        while (this->clientManager->status)
        {
            if (left)
            {
                command nwcmd;
                nwcmd.stillLive(left);
                clientManager->commandQMutexS.lock();
                clientManager->commandsQsend.push(nwcmd);
                clientManager->commandQMutexS.unlock();
            }

            if (right)
            {
                command nwcmd;
                nwcmd.stillLive(right);
                clientManager->commandQMutexS.lock();
                clientManager->commandsQsend.push(nwcmd);
                clientManager->commandQMutexS.unlock();
            }
            Sleep(1000);
        }
    }

    bool heartBeat = false;             // is HeartBeat activated
    int heartBeatTime = 0;              // time to make beat
    int pid, port;                      // pid and port of this node
    Node *father;                       // father node
    Node *left;                         // left son
    Node *right;                        // tight son
    manager *clientManager;             // manager of receiveing and sending messages
    std::map<std::string, int> mapa;    // container for dictonary
    std::chrono::duration<float> dur;   // how much time pass from last beat
    std::chrono::duration<float> fPing; // how Long Without Father Answer
};

int main(int argv, char **argc)
{
    int pid, port, fatherPid, fatherPort;
    if (argv > 2)
    {
        pid = std::atoi(argc[1]);
        port = std::atoi(argc[2]);
        fatherPid = std::atoi(argc[3]);
        fatherPort = std::atoi(argc[4]);
    }
    else
    {
        std::cout << "Client creating: Not enough arguments\n";
        return 0;
    }
    client myclinet(port, pid, fatherPort, fatherPid);
    myclinet.Start();
    std::cout << "client: exit...\n";
    return 0;
}
