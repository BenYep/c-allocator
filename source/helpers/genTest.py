#!/usr/bin/python

import sys
import random

def main():
    args = sys.argv[1:]
    file = open("test", "w")
    list = []
    id = 0
    for i in range(int(args[0])):
        x = random.random()
        if x < 0.50:
            list.append(id)
            file.write("a " + str(id) + " " + str(random.randint(1, 1000)) + "\n")
            id += 1
        elif x < 0.90:
            if len(list) > 0:
                i = random.randint(0, len(list) - 1)
                file.write("f " + str(list[i]) + "\n")
                list.pop(i)
        else:
            if len(list) > 0:
                i = random.randint(0, len(list) - 1)
                file.write("r " + str(list[i]) + " " + str(random.randint(1, 1000)) + "\n")
    for elem in list:
        file.write("f " + str(elem) + "\n")
    file.close()

if __name__ == '__main__':
  main()
