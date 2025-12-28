all: main shaders 

run:
	./run.sh

main:
	mkdir -p build
	gcc main.c -lSDL3 -o ./build/GPU -Wall -Wno-macro-redefined -O3 -ffast-math

shaders: ./*
	mkdir -p build
	@$(foreach file, $(wildcard ./*.*.hlsl), shadercross $(file) -o ./build/$(basename $(file)).spv; echo $(file).spv;)
	@$(foreach file, $(wildcard ./*.*.hlsl), shadercross $(file) -o ./build/$(basename $(file)).msl; echo $(file).msl;)
	@$(foreach file, $(wildcard ./*.*.hlsl), shadercross $(file) -o ./build/$(basename $(file)).dxil; echo $(file).dxil;)

clean:
	rm -rf build
