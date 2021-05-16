U_FAT32_DIR="../fat32-fuse"
#U_FAT32="${U_FAT32_DIR}/fat32.img"
U_FAT32="/dev/sdb1"

if test -e ${U_FAT32_DIR}/fs
then 
    echo "already have fs"
else
    mkdir ${U_FAT32_DIR}/fs
fi

sudo mount ${U_FAT32} ${U_FAT32_DIR}/fs
sudo rm ${U_FAT32_DIR}/fs/*
for programname in $(ls ../user/riscv64)
do 
    sudo cp ../user/riscv64/$programname ${U_FAT32_DIR}/fs/"testsuites_$programname"
done
for programname in $(ls ../user/src/bin)
do
    sudo cp ../user/target/riscv64gc-unknown-none-elf/release/${programname%.rs} ../fat32-fuse/fs/${programname%.rs}
done

sudo umount ${U_FAT32_DIR}/fs
