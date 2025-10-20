LIBS = -framework OpenGL -framework GLUT -framework Carbon -L/usr/local/lib -lfftw3f
#INCLUDES = -I $(HOME)/vcpkg/installed/arm64-osx/include
INCLUDES = -I/usr/local/include
EXTRAS = -std=c++23 -fexperimental-library -march=native -mtune=native -D _LIBCPP_ENABLE_EXPERIMENTAL -D GLM_ENABLE_EXPERIMENTAL -D GLM_FORCE_NEON -Wno-explicit-specialization-storage-class -Wno-macro-redefined -Wno-deprecated-declarations
# -stdlib=libc++ 

build:
	clang++ *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS)
	clang++ *.cpp $(LIBS) $(INCLUDES) $(EXTRAS)

run: 
	clang++ *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS)
	./irradiance

debug:
	clang++ -g *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS) -O0 -g -fsanitize=address -fno-omit-frame-pointer
	./irradiance -width=300 -height=300 -bounces=5 -samples=1

release:
	clang++ -O3 *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS) 
	./irradiance -width=300 -height=300 -bounces=5 -samples=1

linkedin:
	clang++ -O3 *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS) -D CORNELL
	./irradiance -width=800 -height=200 -bounces=5 -samples=1 

hires:
	clang++ -O3 *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS)
	./irradiance -width=1280 -height=720 -bounces=5 -samples=1 -hires

cornell:
	clang++ -O3 *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS) -D CORNELL
	./irradiance -width=400 -height=300 -bounces=5 -samples=1

cornell2:
	clang++ -O3 *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS) -D CORNELL2
	./irradiance -width=400 -height=300 -bounces=5 -samples=1

clean:
	rm -f irradiance
	rm -rf *.o
	rm -f cpp_*.png