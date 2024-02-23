# https://www.cnblogs.com/__tudou__/p/13322854.html
find . -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' ! -regex '.*\(pb\.h\|pb\.cc\)$' -exec clang-format -style=file -i {} \;