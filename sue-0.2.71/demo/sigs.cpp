// +-------------------------------------------------------------------------+
// |              (S)imple (U)nix (E)vents vers. 0.2.71                      |
// | Copyright (c) Andrey Vikt. Stolyarov <crocodil_AT_croco.net> 2003-2008. |
// | ----------------------------------------------------------------------- |
// | This is free software.  Permission is granted to everyone to use, copy  |
// |        or modify this software under the terms and conditions of        |
// |                 GNU LESSER GENERAL PUBLIC LICENSE, v. 2.1               |
// | as published by Free Software Foundation      (see the file LGPL.txt)   |
// |                                                                         |
// | Please visit http://www.croco.net/software/sue for a fresh copy of Sue. |
// | ----------------------------------------------------------------------- |
// |   This code is provided strictly and exclusively on the "AS IS" basis.  |
// | !!! THERE IS NO WARRANTY OF ANY KIND, NEITHER EXPRESSED NOR IMPLIED !!! |
// +-------------------------------------------------------------------------+




/*
   This demo program waits for a signal. Upon SIGINT (Ctrl-C) it 
   prints a message telling you how much times you did this and
   advicing you to press Ctrl-\ instead (to generate SIGQUIT). 

   Having SIGQUIT, it exits.
*/

#include <stdio.h>
#include <signal.h>
#include "sue_sel.hpp"

class SigquitHandler : public SUESignalHandler {
    SUEEventSelector *selector;
public:
    SigquitHandler(SUEEventSelector *a_sel) 
      : SUESignalHandler(SIGQUIT) { selector = a_sel; }
    ~SigquitHandler() {}
    virtual void SignalHandle() { selector->Break(); }
};

class SigintHandler : public SUESignalHandler {
    int count;
public:
    SigintHandler() : SUESignalHandler(SIGINT) { count = 0; }
    ~SigintHandler() {}
    virtual void SignalHandle() {  
        printf("You've pressed Ctrl-C %d time(s). Press Ctrl-\\ instead!\n", 
               ++count);
    }
};

int main() 
{
    SUEEventSelector selector;
    SigquitHandler quithandler(&selector);
    SigintHandler inthandler;
    selector.RegisterSignalHandler(&quithandler);
    selector.RegisterSignalHandler(&inthandler);
    printf("Entering the main loop... \n");
    selector.Go();
    printf("Main loop exited\n");
    return 0;
}


