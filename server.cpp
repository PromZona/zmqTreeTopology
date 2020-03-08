#include "zmq.hpp"
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include "command.h"

#include <windows.h>

class server
{
public:
    server()
    {
    }

    ~server()
    {
    }

    void Start()
    {
        context = zmq::context_t(1);
        recSocket = zmq::socket_t(context, ZMQ_REP);
        std::string path = "tcp://*:";
        path.append(std::to_string(SERVER_PORT));
        recSocket.bind(path);

        std::thread input(&this->cmdCntrl, this);
        std::thread sender(&this->commandSender, this);
        std::thread rec(&this->commandReceiver, this);
        std::thread prc(&this->processor, this);
        std::thread rootPinger(&this->pingRoot, this);

        input.join();
        sender.join();
        prc.join();
        rootPinger.join();
        rec.detach();
        bitCheck.detach();
    }

    void cmdCntrl()
    {
        std::string input;
        int commandid = 0;
        int pid = 0;

        while (input != "Exit")
        {
            std::cin >> input;
            if (input.substr(0, 6) == "create")
            {
                commandid = 1;
            }
            else if (input.substr(0, 6) == "remove")
            {
                commandid = 2;
            }
            else if (input.substr(0, 4) == "exec")
            {
                commandid = 3;
            }
            else if (input.substr(0, 8) == "heartbit")
            {
                commandid = 4;
            }
            else if (input.substr(0, 6) == "status")
            {
                commandid = 9;
            }

            switch (commandid)
            {
            case 1:
            {
                std::cin >> pid;
                bool pidExist = false;
                for (int i = 0; i < pids.size(); i++)
                {
                    if (pid == pids[i])
                    {
                        pidExist = true;
                    }
                }

                if (pidExist)
                {
                    std::cout << "Error: Already exists\n";
                    break;
                }

                if (rootExist == false)
                {
                    std::string procname = "client.exe";
                    std::string cmdArgs = procname;
                    cmdArgs.append(" ");
                    cmdArgs.append(std::to_string(pid));
                    cmdArgs.append(" ");
                    cmdArgs.append(std::to_string(freePort));
                    cmdArgs.append(" 0 ");                       // Server pid
                    cmdArgs.append(std::to_string(SERVER_PORT)); // Server port
                    char *cmdArgschar = new char[cmdArgs.length() + 1];
                    strcpy(cmdArgschar, cmdArgs.c_str());

                    if (CreateProcessA(procname.c_str(), cmdArgschar, NULL, NULL,
                                       FALSE, 0, NULL, NULL, &this->info, &this->processInfo))
                    {
                        std::cout << "Ok: " << pid << std::endl;
                    }
                    else
                    {
                        std::cout << "Cannot create proccess - " << GetLastError() << std::endl;
                    }
                    root = new Node(pid, freePort);
                    rootMutex.lock();
                    rootExist = true;
                    rootMutex.unlock();
                    delete[] cmdArgschar;
                }
                else
                {
                    command newcommand(nullptr, pid, freePort, 0, SERVER_PORT);

                    commandQMutex.lock();
                    commandsQ.push(newcommand);
                    commandQMutex.unlock();
                }
                pids.push_back(pid);
                freePort++;
                break;
            }
            case 2:
            {
                std::cin >> pid;
                command newcommand(root, pid);
                commandQMutex.lock();
                commandsQ.push(newcommand);
                commandQMutex.unlock();

                for (int i = 0; i < pids.size(); i++)
                {
                    if (pid == pids[i])
                    {
                        pids.erase(pids.begin() + i);
                    }
                }

                Sleep(SLEEP_TIME);
                if (pid == root->pid)
                {
                    delete root;
                    root = nullptr;
                    rootExist = false;
                }
                break;
            }
            case 3:
            {
                std::string a;
                std::getline(std::cin, a);
                char str[MAX_ARGS_SIZE];
                int num = -1;
                sscanf(a.c_str(), " %d %s %d", &pid, str, &num);
                if (num == -1)
                {
                    command nwcom(root, pid, str);
                    commandQMutex.lock();
                    commandsQ.push(nwcom);
                    commandQMutex.unlock();
                }
                else
                {
                    command nwcom(root, pid, str, num);
                    commandQMutex.lock();
                    commandsQ.push(nwcom);
                    commandQMutex.unlock();
                }
                break;
            }
            case 4:
            {
                int time = 0;
                std::cin >> time;
                if (time < 100)
                {
                    std::cout << "ERROR: too little time. Heartbit at least 100\n";
                    break;
                }
                command nwcommand;
                nwcommand.heartbit(root, time);
                commandQMutex.lock();
                commandsQ.push(nwcommand);
                commandQMutex.unlock();
                bitTime = time;
                bitCheck = std::thread(&this->bitter, this);
                std::cout << "0: HEARTBIT ACTIVATED\n";
                break;
            }
            case 9:
            {
                DWORD buf = 0;
                GetExitCodeProcess(processInfo.hProcess, &buf);
                std::cout << "Child Status: " << buf << std::endl;
                buf = GetLastError();
                std::cout << "Last error: " << buf << std::endl;
                break;
            }
            default:
                std::cout << "I dont know this command\n";
                break;
            }
        }
    }

    void commandSender()
    {
        while (serverStatus)
        {
            if (commandsQ.size() > 0)
            {
                commandQMutex.lock();
                command commandInProccess = commandsQ.front();
                commandsQ.pop();
                commandQMutex.unlock();
                std::string stringCommand;
                stringCommand.append(std::to_string(commandInProccess.commandCode));
                stringCommand.append(" ");
                stringCommand.append(commandInProccess.args);
                zmq::message_t package(stringCommand.size());
                memcpy(package.data(), stringCommand.data(), stringCommand.size());
                try
                {
                    root->respondSocket.connect(root->respondPath.c_str());
                    if (root->respondSocket.send(package))
                    {
                    }
                    else
                        std::cout << "Server: pckg NOT sended\n";

                    if (root->respondSocket.recv(&package))
                    {
                        stringCommand = std::string(static_cast<char *>(package.data()), package.size());
                    }
                    else
                    {
                        std::cout << "Server: message from client dont receive\n";
                    }
                }
                catch (zmq::error_t &e)
                {
                    std::cout << "Server: Error - " << e.what() << std::endl;
                }
            }
            Sleep(50);
        }
    }

    void commandReceiver()
    {
        try
        {
            while (serverStatus)
            {

                zmq::message_t received, sendbuf(2);
                recSocket.recv(&received);       // Receive
                memcpy(sendbuf.data(), "Ok", 2); // Ok
                recSocket.send(sendbuf);         // Reply

                std::string mes = std::string(static_cast<char *>(received.data()), received.size());
                int cmdCode = 0;
                char args[MAX_ARGS_SIZE];
                sscanf(mes.c_str(), "%d %[^\n]", &cmdCode, args);

                command commandBuffer = command(nullptr, (char)cmdCode, std::string(args));
                commandReceived.push(commandBuffer);
            }
        }
        catch (zmq::error_t &e)
        {
            std::cout << "Manager: Error - " << e.what() << std::endl;
        }
    }

    void processor()
    {
        while (serverStatus)
        {
            if (commandReceived.size() > 0)
            {
                command cmdProc = commandReceived.front();
                commandReceived.pop();
                switch (cmdProc.commandCode)
                {
                case 6:
                {
                    pidsTime.find(atoi(cmdProc.args.c_str()))->second = 0;
                    break;
                }
                }
            }
            Sleep(SLEEP_TIME);
        }
    }

    void bitter()
    {

        for (int i = 0; i < pids.size(); i++)
        {
            pidsTime.insert(std::pair<int, int>(pids[i], 0));
        }

        int waittime = 25;
        while (serverStatus)
        {
            Sleep(waittime);
            for (auto i = pidsTime.begin(); i != pidsTime.end();)
            {
                i->second += waittime;
                if (i->second >= bitTime * 4)
                {
                    std::cout << "Heartbit: node " << i->first << " is unavailable now\n";
                    pidsTime.erase(i++);
                }
                else
                {
                    ++i;
                }
            }
        }
    }

    void pingRoot()
    {
        while (this->serverStatus)
        {
            if (rootExist)
            {
                command nwcmd;
                nwcmd.stillLive(root);
                this->commandQMutex.lock();
                this->commandsQ.push(nwcmd);
                this->commandQMutex.unlock();
            }
            Sleep(1000);
        }
    }

    zmq::context_t context;
    zmq::socket_t recSocket;
    std::queue<command> commandsQ;
    std::queue<command> commandReceived;
    STARTUPINFO info = {sizeof(info)};
    PROCESS_INFORMATION processInfo;
    std::mutex rootMutex;     // Mutex for root variables
    std::mutex commandQMutex; // Mutex for command queue
    Node *root;
    int freePort = 5555;
    int rootPort = 5555;
    const int SERVER_PORT = 5554;
    bool rootExist = false;
    bool serverStatus = true;
    std::vector<int> pids;

    std::thread bitCheck;
    std::map<int, int> pidsTime; // <pid, time>
    int bitTime = 0;
};

int main()
{
    server myserver;
    myserver.Start();

    return 0;
}

/*
    //  Prepare our context and socket
    zmq::context_t rootcontext(1);
    zmq::socket_t rootsocket(rootcontext, ZMQ_REP);
    rootsocket.bind("tcp://*:5555");
    while (true)
    {
        zmq::message_t request;
        //  Wait for next request from client
        rootsocket.recv(&request);
        std::cout << "Received Hello" << std::endl;

        //  Do some 'work'
        sleep(1);

        //  Send reply back to client
        zmq::message_t reply(5);
        memcpy(reply.data(), "World", 5);
        rootsocket.send(reply);
    }
*/