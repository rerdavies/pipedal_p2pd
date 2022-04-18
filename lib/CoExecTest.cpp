/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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