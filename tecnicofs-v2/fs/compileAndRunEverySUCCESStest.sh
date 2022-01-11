passed=0;
total=0;

echo "compiling...";

gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o operations.o operations.c;

gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o state.o state.c;


echo "testar os testes gerais"
for test in tests/SUCCESS*.c; do
	gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o testBeingTested.o $test;

	gcc -pthread operations.o state.o testBeingTested.o -o currentTest;

	echo "running test $test...";

	output=$(./currentTest);
	echo $output;

	if [ `echo $output | grep -c "Sucessful" ` -gt 0 -o `echo $output | grep -c "Successful" ` -gt 0 ]
	then
  		passed=$((passed+1));
	else
  		echo "ERROUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU";
	fi


	echo "";

	rm testBeingTested.o;

	total=$((total+1));
done


echo "";echo "";echo "";
echo "testar os testes da amiga do Pedro";
for test in TestesSofia/SUCCESS*.c; do
	gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Ifs -I. -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wno-sign-compare -g -O3 -c -o testBeingTested.o $test;

	gcc -pthread operations.o state.o testBeingTested.o -o currentTest;

	echo "running test $test...";

	
	output=$(./currentTest);
	echo $output;

	if [ `echo $output | grep -c "Sucessful" ` -gt 0 -o `echo $output | grep -c "Successful" ` -gt 0 ]
	then
  		passed=$((passed+1));
	else
  		echo "ERROUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU";
	fi



	echo "";

	rm testBeingTested.o;

	total=$((total+1));
done


rm *.o;

echo "Passámos $passed/$total testes :D"