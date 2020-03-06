#include <string>
#include <functional>
#include <windows.h>
#include <map>

#define sleep(n) Sleep(n)

#define MAX_ARGS_SIZE 128

class Node
{
public:
    Node(int p, int pt)
    {
        pid = p;
        port = pt;
        respondContext = zmq::context_t(1);
        respondSocket = zmq::socket_t(respondContext, ZMQ_REQ);
        respondPath = std::string("tcp://localhost:");
        respondPath.append(std::to_string(port));
    }

    void print()
    {
        std::cout << "NODE: " << this << " pid: " << pid << " port: " << port << " path: " << respondPath << std::endl;
    }

    int pid;                                     // Node pid
    int port;                                    // Node port
    zmq::context_t respondContext;               // context to send
    zmq::socket_t respondSocket;                 // Socket to connect
    std::string respondPath;                     // path to connect
    STARTUPINFO strupInfo = {sizeof(strupInfo)}; // father node know startUp info
    PROCESS_INFORMATION processInfo;             // and process info
};

class command
{
public:
    command()
    {
    }

    // Command for creating new client [1]
    command(Node *node, int clientPid, int clientPort, int fatherPid, int fatherPort)
    {
        commandCode = 1;
        nodeP = node;
        args = std::to_string(clientPid);
        args.append(" ");
        args.append(std::to_string(clientPort));
        args.append(" ");
        args.append(std::to_string(fatherPid));
        args.append(" ");
        args.append(std::to_string(fatherPort));
    }

    // Command for remove client [2]
    command(Node *node, int clientPid)
    {
        commandCode = 2;
        nodeP = node;
        args = std::to_string(clientPid);
    }

    // if args is in message UNIVERSALE
    command(Node *node, char cmdCode, std::string nargs)
    {
        commandCode = cmdCode;
        nodeP = node;
        args = nargs;
    }

    // Node dont available [-1]
    command(Node *node)
    {
        nodeP = node;
        commandCode = -1;
    }

    // if need to add in dict [3]
    command(Node *node, int clientPid, std::string str, int num)
    {
        nodeP = node;
        commandCode = 3;
        args = std::to_string(clientPid);
        args.append(" ");
        args.append(str);
        args.append(" ");
        args.append(std::to_string(num));
    }

    // if need to print from dict [4]
    command(Node *node, int clientPid, std::string str)
    {
        nodeP = node;
        commandCode = 4;
        args = std::to_string(clientPid);
        args.append(" ");
        args.append(str);
    }

    // Heartbit activate [5]
    void heartbit(Node *node, int clientPid)
    {
        nodeP = node;
        commandCode = 5;
        args = std::to_string(clientPid);
    }

    // send bit to server [6]
    void bit(Node *node, int pid)
    {
        nodeP = node;
        commandCode = 6;
        args = std::to_string(pid);
        isFather = true;
    }

    void print()
    {
        std::cout << "command: " << (int)commandCode << std::endl;
        std::cout << "args: " << args << std::endl;
        nodeP->print();
    }
    bool isFather = false;
    char commandCode;
    std::string args;
    Node *nodeP; // Node where to send
};

class manager
{
public:
    manager()
    {
    }

    ~manager()
    {
    }

    void Start()
    {
        receiverContext = zmq::context_t(1);
        receiverSocket = zmq::socket_t(receiverContext, ZMQ_REP);
        std::string path = "tcp://*:";
        path.append(std::to_string(port));
        receiverSocket.bind(path);

        std::thread input(&this->commandReceiver, this);
        std::thread sender(&this->commandSender, this);

        sender.join();
        input.detach();
    }

    void commandReceiver()
    {
        try
        {
            while (status)
            {

                zmq::message_t received, sendbuf(2);
                receiverSocket.recv(&received);  // Receive
                memcpy(sendbuf.data(), "Ok", 2); // Ok
                receiverSocket.send(sendbuf);    // Reply
                std::string mes = std::string(static_cast<char *>(received.data()), received.size());
                int cmdCode = 0;
                char args[MAX_ARGS_SIZE];
                sscanf(mes.c_str(), "%d %[^\n]", &cmdCode, args);
                command commandBuffer = command(nullptr, (char)cmdCode, std::string(args));
                commandQMutexR.lock();
                commandsQreceived.push(commandBuffer);
                commandQMutexR.unlock();
            }
        }
        catch (zmq::error_t &e)
        {
            std::cout << "Manager: Error - " << e.what() << std::endl;
        }
    }

    void commandSender()
    {
        while (status)
        {
            commandQMutexS.lock();
            if (commandsQsend.size() > 0)
            {

                command cmdProc = commandsQsend.front();
                commandsQsend.pop();

                if (!cmdProc.isFather)
                {
                    DWORD stat;
                    GetExitCodeProcess(cmdProc.nodeP->processInfo.hProcess, &stat);
                    if (stat != 259)
                    {
                        std::cout << "Error: pid " << cmdProc.nodeP->pid << " unavailable\n";
                        command nwcmd(cmdProc.nodeP);

                        commandQMutexR.lock();
                        commandsQreceived.push(nwcmd);
                        commandQMutexR.unlock();
                        continue;
                    }
                }

                std::string stringCommand;
                stringCommand.append(std::to_string(cmdProc.commandCode));
                stringCommand.append(" ");
                stringCommand.append(cmdProc.args);
                zmq::message_t package(stringCommand.size());
                memcpy(package.data(), stringCommand.data(), stringCommand.size());

                try
                {
                    cmdProc.nodeP->respondSocket.connect(cmdProc.nodeP->respondPath.c_str());
                    cmdProc.nodeP->respondSocket.send(package);
                    cmdProc.nodeP->respondSocket.recv(&package);
                    stringCommand = std::string(static_cast<char *>(package.data()), package.size());
                    cmdProc.nodeP->respondSocket.disconnect(cmdProc.nodeP->respondPath.c_str());
                }
                catch (zmq::error_t &e)
                {
                    std::cout << "Manager: Error - " << e.what() << std::endl;
                }
            }
            commandQMutexS.unlock();
            sleep(1000);
        }
    }

    zmq::context_t receiverContext;
    zmq::socket_t receiverSocket;
    std::queue<command> commandsQreceived;     // Meesages that need to be process
    std::queue<command> commandsQsend;         // Messages to Send
    std::mutex commandQMutexR, commandQMutexS; // Mutex for command queue
    bool status = true;
    int port = 0;
};