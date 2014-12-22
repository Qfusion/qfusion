find . -name "*.c" -o -name "*.cpp" -o -name "*.h" | while read file; do
	echo "${file}";
	clang-format -style=file -i "${file}";
done