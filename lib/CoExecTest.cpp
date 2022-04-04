#include "cotask/CoExec.h"

using namespace cotask;
using namespace std;

CoTask<> CoForwardInput(CoFile &output)
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

CoTask<> CoForwardStdOut(CoFile &output, int &openOutputs)
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
    cout << "StdOut done." << endl;
    if (--openOutputs)
    {
        CoDispatcher::CurrentDispatcher().PostQuit();
    }
}
CoTask<> CoForwardStdErr(CoFile &output, int &openOutputs)
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
    cout << "Stderr done." << endl;
    if (--openOutputs)
    {
        CoDispatcher::CurrentDispatcher().PostQuit();
    }
}

int main(int argc, char **argv)
{
    std::vector<std::string> arguments;

    for (int i = 1; i < argc; ++i)
    {
        arguments.push_back(argv[i]);
    }
    CoExec asyncExec;

    try
    {
        CoDispatcher &dispatcher = CoDispatcher::CurrentDispatcher();

        asyncExec.Execute("bash", arguments);

        dispatcher.StartThread(CoForwardInput(asyncExec.Stdin()));

        int openOutputs = 2;
        dispatcher.StartThread(CoForwardStdOut(asyncExec.Stdout(),openOutputs));
        dispatcher.StartThread(CoForwardStdErr(asyncExec.Stderr(),openOutputs));

        dispatcher.MessageLoop();
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
    }

    Dispatcher().DestroyDispatcher();
    return 0;
}