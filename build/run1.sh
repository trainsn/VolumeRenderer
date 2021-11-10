#PBS -N MPASMap 
#PBS -l walltime=1:00:00
#PBS -l nodes=1:ppn=1
#PBS -j oe

make

./MPASMap BwsA0.00_.nc
./MPASMap BwsA1.00_.nc
./MPASMap BwsA3.00_.nc
./MPASMap BwsA5.00_.nc