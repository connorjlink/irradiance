LIBS = -framework OpenGL -framework GLUT -framework Carbon
#INCLUDES = -I $(HOME)/vcpkg/installed/arm64-osx/include
INCLUDES = 
EXTRAS = -std=c++23 -fopenmp -fexperimental-library -march=native -mtune=native -D _LIBCPP_ENABLE_EXPERIMENTAL -D GLM_ENABLE_EXPERIMENTAL -D GLM_FORCE_NEON -Wno-explicit-specialization-storage-class -Wno-macro-redefined
# -stdlib=libc++ 

build:
	clang++ *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS)
	clang++ *.cpp $(LIBS) $(INCLUDES) $(EXTRAS)

run: 
	clang++ *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS)
	./irradiance

release:
	clang++ -O3 *.cpp -o irradiance $(LIBS) $(INCLUDES) $(EXTRAS)
	./irradiance -width=500 -height=500 -bounces=3 -samples=1

clean:
	rm -f irradiance
	rm -rf *.o