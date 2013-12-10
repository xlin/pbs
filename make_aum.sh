g++ -Wall -Werror -pedantic -pedantic-errors -Wformat -Wformat-security \
    -lmysqlclient -lboost_system -lboost_filesystem -lboost_regex -lboost_thread  -lpthread \
    main_aum.cpp cBuild.cpp functions.cpp cChroot.cpp \
    -o aum++
