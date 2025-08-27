LIBS = -framework OpenGL -framework GLUT -framework Carbon
#INCLUDES = -I $(HOME)/vcpkg/installed/arm64-osx/include
INCLUDES = 

build:
	clang++ *.cpp -o irradiance -std=c++26 $(LIBS) $(INCLUDES)

run: 
	clang++ *.cpp -o irradiance -std=c++26 $(LIBS) $(INCLUDES)
	./irradiance

release:
	clang++ -O3 *.cpp -o irradiance -std=c++26 $(LIBS) $(INCLUDES)
	./irradiance -width=500 -height=500 -bounces=2 -samples=2

clean:
	rm -f irradiance
	rm -rf *.o