# well, what the hell...
</$objtype/mkfile

all: NinePea.$O doc/paper.pdf
	mk NinePea.$O doc/paper.pdf

NinePea.$O: NinePea.cpp NinePea.h
	$O^c -D Plan9 -o NinePea.$O NinePea.cpp

doc/paper.pdf: doc/paper.ms
	cd doc; mk paper.pdf; cd ..

clean:
	rm -f NinePea.$O doc/paper.pdf
