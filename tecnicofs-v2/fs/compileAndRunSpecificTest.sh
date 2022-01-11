echo "testing $1";
echo "...";
echo "compiling...";

gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o operations.o operations.c;

gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o state.o state.c;

gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o testBeingTested.o $1;

gcc -pthread operations.o state.o testBeingTested.o -o currentTest;

echo "running...";

./currentTest;

rm *.o;