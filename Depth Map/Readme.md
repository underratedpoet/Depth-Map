https://www.glfw.org/download.html
������ ���� � ������������ ������ � ����������� GLFW:
Project -> Properties -> C/C++ -> General -> Additional Include Directories: ������ ���� � ����� include �� �������������� ������ GLFW.
Project -> Properties -> Linker -> General -> Additional Library Directories: ������ ���� � ����� lib-vc2019 (��� ������, ��������������� ����� ������ Visual Studio) �� �������������� ������ GLFW.
Project -> Properties -> Linker -> Input -> Additional Dependencies: ������ glfw3.lib.

https://glad.dav1d.de/
������ ����� glad.c � ���� ������ (������ ������� �� ������ -> Add -> Existing Item).
������ ���� � ������������ ������ GLAD:
Project -> Properties -> C/C++ -> General -> Additional Include Directories: ������ ���� � ����� include �� �������������� ������ GLAD.