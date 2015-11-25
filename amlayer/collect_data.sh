make clean > /dev/null
make > /dev/null
f="file"
g="testrel.0"
for x in {10,100,1000,2000,3000,4000,5000,7000,10000}
do	
	for i in $(seq 1 $x)
	do
		echo $RANDOM >>file
	done
	rm testrel.0
	./bulk_load.out file
	
	if [[ -f $f ]]
		then
		rm file
	fi
	# rm $g
done