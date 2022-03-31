#include "p2psession/WpaCliWrapper.h"

using namespace p2psession;
using namespace std;

CoTask<> CoForwardInput(AsyncFile &output)
{
    try
    {
        std::string t = "echo hello world!\n";
        co_await output.CoWriteLine("echo Hello world!");

        co_await output.CoWriteLine("ls -l");
        co_await output.CoWriteLine("exit");
    }
    catch (const std::exception &e)
    {
        cout << "Output error: " << e.what() << endl;
    }
    cout << "Input done." << endl;
}

CoTask<> CoForwardOutput(AsyncFile &output)
{
    try
    {
        std::string line;
        while (true)
        {
            if (!co_await output.CoReadLine(&line))
            {
                break;
            };
            cout << ": " << line << endl;
        }
    }
    catch (const std::exception &e)
    {
        cout << "ForwardOutput: " << e.what() << endl;
    }
    cout << "Output done." << endl;
    CoDispatcher::CurrentDispatcher().PostQuit();
}
int main(int argc, char **argv)
{
    std::vector<std::string> arguments;

    for (int i = 1; i < argc; ++i)
    {
        arguments.push_back(argv[i]);
    }
    AsyncExec asyncExec;

    try
    {
        CoDispatcher &dispatcher = CoDispatcher::CurrentDispatcher();

        asyncExec.Execute("bash", arguments);


        dispatcher.StartThread(CoForwardInput(asyncExec.Stdin()));

        dispatcher.StartThread(CoForwardOutput(asyncExec.Stdout()));

        dispatcher.MessageLoop();
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
    }

    Dispatcher().DestroyDispatcher();
    return 0;

}