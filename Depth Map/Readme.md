https://www.glfw.org/download.html

Добавь пути к заголовочным файлам и библиотекам GLFW:

Project -> Properties -> C/C++ -> General -> Additional Include Directories: добавь путь к папке include из распакованного архива GLFW.

Project -> Properties -> Linker -> General -> Additional Library Directories: добавь путь к папке lib-vc2019 (или другой, соответствующей твоей версии Visual Studio) из распакованного архива GLFW.

Project -> Properties -> Linker -> Input -> Additional Dependencies: добавь glfw3.lib.



https://glad.dav1d.de/

Добавь файлы glad.c в свой проект (правой кнопкой на проект -> Add -> Existing Item).

Добавь пути к заголовочным файлам GLAD:

Project -> Properties -> C/C++ -> General -> Additional Include Directories: добавь путь к папке include из распакованного архива GLAD.
