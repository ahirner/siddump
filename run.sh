if gcc sidparse.c cpu.c -o sidparse ; then
    ./sidparse $@ -b
else
    echo "Compile failed"
fi
