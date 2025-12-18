with open("/tmp/io300_input", 'r+', encoding='latin-1') as f:
    byte = f.read(1)
    count = 0
    while byte:
        count+=1
        byte = f.read(1)


print(count)
