#include "Render.h"
#include <Windows.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "GUItextRectangle.h"

#include <vector>
#include <math.h>

#define PI 3.1415927

using namespace std;

void draw_cylinder(double radius, double height, double color[]);
void pyro();


#ifdef _DEBUG
#include <Debugapi.h> 
struct debug_print
{
	template<class C>
	debug_print& operator<<(const C& a)
	{
		OutputDebugStringA((std::stringstream() << a).str().c_str());
		return *this;
	}
} debout;
#else
struct debug_print
{
	template<class C>
	debug_print& operator<<(const C& a)
	{
		return *this;
	}
} debout;
#endif

//библиотека для разгрузки изображений
//https://github.com/nothings/stb
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = false;


//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL* sender, KeyEventArg arg)
{
	//конвертируем код клавиши в букву
	auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

	switch (key)
	{
	case 'L':
		lightning = !lightning;
		break;
	case 'T':
		texturing = !texturing;
		break;
	case 'A':
		alpha = !alpha;
		break;
	}
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером
void initRender()
{
	//==============НАСТРОЙКА ТЕКСТУР================
	//4 байта на хранение пикселя
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	//просим сгенерировать нам Id для текстуры
	//и положить его в texId
	glGenTextures(1, &texId);

	//делаем текущую текстуру активной
	//все, что ниже будет применено texId текстуре.
	glBindTexture(GL_TEXTURE_2D, texId);


	int x, y, n;

	//загружаем картинку
	//см. #include "stb_image.h" 
	unsigned char* data = stbi_load("texture.png", &x, &y, &n, 4);
	//x - ширина изображения
	//y - высота изображения
	//n - количество каналов
	//4 - нужное нам количество каналов
	//пиксели будут хранится в памяти [R-G-B-A]-[R-G-B-A]-[..... 
	// по 4 байта на пиксель - по байту на канал
	//пустые каналы будут равны 255

	//Картинка хранится в памяти перевернутой 
	//так как ее начало в левом верхнем углу
	//по этому мы ее переворачиваем -
	//меняем первую строку с последней,
	//вторую с предпоследней, и.т.д.
	unsigned char* _tmp = new unsigned char[x * 4]; //времянка
	for (int i = 0; i < y / 2; ++i)
	{
		std::memcpy(_tmp, data + i * x * 4, x * 4);//переносим строку i в времянку
		std::memcpy(data + i * x * 4, data + (y - 1 - i) * x * 4, x * 4); //(y-1-i)я строка -> iя строка
		std::memcpy(data + (y - 1 - i) * x * 4, _tmp, x * 4); //времянка -> (y-1-i)я строка
	}
	delete[] _tmp;


	//загрузка изображения в видеопамять
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	//выгрузка изображения из опперативной памяти
	stbi_image_free(data);


	//настройка режима наложения текстур
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//GL_REPLACE -- полная замена политога текстурой
//настройка тайлинга
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//настройка фильтрации
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//======================================================

	//================НАСТРОЙКА КАМЕРЫ======================
	camera.caclulateCameraPos();

	//привязываем камеру к событиям "движка"
	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
	//==============НАСТРОЙКА СВЕТА===========================
	//привязываем свет к событиям "движка"
	gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
	gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
	gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
	//========================================================
	//====================Прочее==============================
	gl.KeyDownEvent.reaction(switchModes);
	text.setSize(512, 180);
	//========================================================


	camera.setPosition(2, 1.5, 1.5);
}

void Render(double delta_time)
{
	glEnable(GL_DEPTH_TEST);

	//натройка камеры и света
	//в этих функциях находятся OGLные функции
	//которые устанавливают параметры источника света
	//и моделвью матрицу, связанные с камерой.

	if (gl.isKeyPressed('F')) //если нажата F - свет из камеры
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	light.SetUpLight();


	//рисуем оси
	gl.DrawAxes();

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);


	//включаем режимы, в зависимости от нажания клавиш. см void switchModes(OpenGL *sender, KeyEventArg arg)
	if (lightning)
		glEnable(GL_LIGHTING);
	if (texturing)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0); //сбрасываем текущую текстуру
	}

	if (alpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	//=============НАСТРОЙКА МАТЕРИАЛА==============


	//настройка материала, все что рисуется ниже будет иметь этот метериал.
	//массивы с настройками материала
	float  amb[] = { 0.2, 0.2, 0.1, 1. };
	float dif[] = { 0.4, 0.65, 0.5, 1. };
	float spec[] = { 0.9, 0.8, 0.3, 1. };
	float sh = 0.2f * 256;

	//фоновая
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//дифузная
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//зеркальная
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	//размер блика
	glMaterialf(GL_FRONT, GL_SHININESS, sh);

	//чтоб было красиво, без квадратиков (сглаживание освещения)
	glShadeModel(GL_SMOOTH); //закраска по Гуро      
			   //(GL_SMOOTH - плоская закраска)

	//============ РИСОВАТЬ ТУТ ==============

	pyro();
	double color[3] = { 0, 0, 0 };
	//draw_cylinder(3.0413812, 5, color);

	//рисуем источник света
	light.DrawLightGizmo();

	//================Сообщение в верхнем левом углу=======================

	//переключаемся на матрицу проекции
	glMatrixMode(GL_PROJECTION);
	//сохраняем текущую матрицу проекции с перспективным преобразованием
	glPushMatrix();
	//загружаем единичную матрицу в матрицу проекции
	glLoadIdentity();

	//устанавливаем матрицу паралельной проекции
	glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

	//переключаемся на моделвью матрицу
	glMatrixMode(GL_MODELVIEW);
	//сохраняем матрицу
	glPushMatrix();
	//сбразываем все трансформации и настройки камеры загрузкой единичной матрицы
	glLoadIdentity();

	//отрисованное тут будет визуалзироватся в 2д системе координат
	//нижний левый угол окна - точка (0,0)
	//верхний правый угол (ширина_окна - 1, высота_окна - 1)


	std::wstringstream ss;
	ss << std::fixed << std::setprecision(3);
	ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
	ss << L"Коорд. света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5) << delta_time << std::endl;


	text.setPosition(10, gl.getHeight() - 10 - 180);
	text.setText(ss.str().c_str());
	text.Draw();

	//восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();


}


struct Point
{
	double x, y, z;
};

Point find_n(Point A, Point B, Point C)
{
	Point BA{ A.x - B.x,A.y - B.y,A.z - B.z };
	Point BC{ C.x - B.x,C.y - B.y,C.z - B.z };
	Point N{ BA.y * BC.z - BA.z * BC.y,
			-BA.x * BC.z + BA.z * BC.x,
			 BA.x * BC.y - BA.y * BC.x };
	double l = sqrt(N.x * N.x + N.y * N.y + N.z * N.z);
	N.x /= l;
	N.y /= l;
	N.z /= l;
	return N;
}
void draw_quad(double color[], double Coord[4][3])
{
	Point A = { Coord[0][0], Coord[0][1], Coord[0][2] };
	Point B = { Coord[1][0], Coord[1][1], Coord[1][2] };
	Point C = { Coord[2][0], Coord[2][1], Coord[2][2] };
	Point D = { Coord[3][0], Coord[3][1], Coord[3][2] };
	Point N = find_n(A, B, C);



	Point center{ (A.x + B.x + C.x + D.x) / 4.0,
				  (A.y + B.y + C.y + D.y) / 4.0,
				  (A.z + B.z + C.z + D.z) / 4.0 };

	Point N_end{ center.x + N.x,
				 center.y + N.y,
				 center.z + N.z };

	bool b_light = glIsEnabled(GL_LIGHTING);
	//отключаем освещение, чтобы раскрасить вектор привычным нам glColor
	if (b_light)
		glDisable(GL_LIGHTING);
	glBegin(GL_LINES);
	glColor3d(1, 0, 0);
	glVertex3dv((double*)&center);
	glVertex3dv((double*)&N_end);
	glEnd();
	//восстанавливаем освещение, если нужно.
	if (b_light)
		glEnable(GL_LIGHTING);

	glNormal3dv((double*)&N);
	glBegin(GL_QUADS);

	glColor3d(color[0], color[1], color[2]);
	glVertex3dv(Coord[0]);
	glVertex3dv(Coord[1]);
	glVertex3dv(Coord[2]);
	glVertex3dv(Coord[3]);
	glEnd();
}
void draw_trian(double color[], double Coord[3][3])
{
	Point A = { Coord[0][0], Coord[0][1], Coord[0][2] };
	Point B = { Coord[1][0], Coord[1][1], Coord[1][2] };
	Point C = { Coord[2][0], Coord[2][1], Coord[2][2] };
	Point N = find_n(A, B, C);

	Point center{ (A.x + B.x + C.x) / 3.0,
				  (A.y + B.y + C.y) / 3.0,
				  (A.z + B.z + C.z) / 3.0 };

	Point N_end{ center.x + N.x,
				 center.y + N.y,
				 center.z + N.z };

	bool b_light = glIsEnabled(GL_LIGHTING);
	//отключаем освещение, чтобы раскрасить вектор привычным нам glColor
	if (b_light)
		glDisable(GL_LIGHTING);
	glBegin(GL_LINES);
	glColor3d(1, 0, 0);
	glVertex3dv((double*)&center);
	glVertex3dv((double*)&N_end);
	glEnd();
	//восстанавливаем освещение, если нужно.
	if (b_light)
		glEnable(GL_LIGHTING);

	glNormal3dv((double*)&N);
	glBegin(GL_TRIANGLES);

	glColor3d(color[0], color[1], color[2]);
	glVertex3dv(Coord[0]);
	glVertex3dv(Coord[1]);
	glVertex3dv(Coord[2]);
	glEnd();
}
void fill_trian_mas(vector<double>& coord, double trian_coord[3][3])
{
	trian_coord[0][0] = coord[0]; trian_coord[0][1] = coord[1]; trian_coord[0][2] = coord[2];
	trian_coord[1][0] = coord[3]; trian_coord[1][1] = coord[4]; trian_coord[1][2] = coord[5];
	trian_coord[2][0] = coord[6]; trian_coord[2][1] = coord[7]; trian_coord[2][2] = coord[8];
}
void fill_quad_mas(vector<double>& coord, double quad_coord[4][3])
{
	quad_coord[0][0] = coord[0]; quad_coord[0][1] = coord[1]; quad_coord[0][2] = coord[2];
	quad_coord[1][0] = coord[3]; quad_coord[1][1] = coord[4]; quad_coord[1][2] = coord[5];
	quad_coord[2][0] = coord[6]; quad_coord[2][1] = coord[7]; quad_coord[2][2] = coord[8];
	quad_coord[3][0] = coord[9]; quad_coord[3][1] = coord[10]; quad_coord[3][2] = coord[11];
}
void draw_cylinder(double radius, double height, double color[])
{
	double angle = 0.0;
	double angle_step = 0.1;
	double x = 0.0;
	double y = 0.0;
	double C = 0.5525684671;
	int k = 0;
	int p = 0;

	glBegin(GL_QUAD_STRIP);
	glColor3d(color[0], color[1], color[2]);
	while (angle < 2 * PI)
	{
		if ((angle >= C * PI) && (angle <= C * PI + PI))
		{
			x = 3.5 + radius * cos(angle);
			y = -5 + radius * sin(angle);
			if (k == 0)
			{
				glVertex3d(3, -2, height);
				glVertex3d(3, -2, 0);
			}
			angle = angle + angle_step;
			++k;
			continue;
		}
		if ((k > 0) && (p == 0))
		{
			glVertex3d(4, -8, height);
			glVertex3d(4, -8, 0);
			p = 1;
		}
		x = 3.5 + radius * cos(angle);
		y = -5 + radius * sin(angle);
		glVertex3d(x, y, height);
		glVertex3d(x, y, 0.0);
		angle = angle + angle_step;
	}
	glVertex3d(radius + 3.5, -5, height);
	glVertex3d(radius + 3.5, -5, 0.0);
	glEnd();
	k = 0;
	p = 0;
	glBegin(GL_POLYGON);
	glColor3d(color[0] + 0.2, color[1] + 0.2, color[2] + 0.2);
	angle = 0.0;
	while (angle < 2 * PI) {
		if ((angle >= C * PI) && (angle <= C * PI + PI))
		{
			x = 3.5 + radius * cos(angle);
			y = -5 + radius * sin(angle);
			if (k == 0)
			{
				glVertex3d(3, -2, height);
			}
			angle = angle + angle_step;
			++k;
			continue;
		}
		if ((k > 0) && (p == 0))
		{
			glVertex3d(4, -8, height);
			p = 1;
		}
		x = 3.5 + radius * cos(angle);
		y = -5 + radius * sin(angle);
		glVertex3f(x, y, height);
		angle = angle + angle_step;
	}
	glVertex3f(radius + 3.5, -5, height);
	glEnd();
	k = 0;
	p = 0;
	glBegin(GL_POLYGON);
	glColor3d(color[0] + 0.2, color[1] + 0.2, color[2] + 0.2);
	angle = 0.0;
	while (angle < 2 * PI) {
		if ((angle >= C * PI) && (angle <= C * PI + PI))
		{
			x = 3.5 + radius * cos(angle);
			y = -5 + radius * sin(angle);
			if (k == 0)
			{
				glVertex3d(3, -2, 0.0);
			}
			angle = angle + angle_step;
			++k;
			continue;
		}
		if ((k > 0) && (p == 0))
		{
			glVertex3d(4, -8, 0);
			p = 1;
		}
		x = 3.5 + radius * cos(angle);
		y = -5 + radius * sin(angle);
		glVertex3f(x, y, 0);
		angle = angle + angle_step;
	}
	glVertex3f(radius + 3.5, -5, 0);
	glEnd();
	k = 0;
	p = 0;
}

void pyro()
{
	double color[3];
	double trian_coord[3][3];
	double quad_coord[4][3];
	vector<double> coord_t(9);
	vector<double> coord_q(12);

	color[0] = 0.3; color[1] = 0.3; color[2] = 1;
	coord_t = { 4, -8, 0, 3, -2, 0, 2, -4, 0 };
	fill_trian_mas(coord_t, trian_coord);
	draw_trian(color, trian_coord);   //ok

	color[0] = 0.1; color[1] = 0.7; color[2] = 0.7;
	coord_t = { 8, 2, 0, 3, 1, 0, 3, -2, 0 };
	fill_trian_mas(coord_t, trian_coord);
	draw_trian(color, trian_coord);  //ok

	color[0] = 0.9; color[1] = 0.1; color[2] = 0.2;
	coord_q = { -2, -5, 0, 2, -4, 0, 3, -2, 0, 3, 1, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord);  //ok

	color[0] = 0.1; color[1] = 0.5; color[2] = 0;
	coord_q = { -2, -5, 0, 3, 1, 0, 1, 7, 0, -6, 3, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord);  //ok

	color[0] = 0; color[1] = 0; color[2] = 0.5;
	coord_q = { 2, -4, 5,  4, -8, 5, 4, -8, 0, 2, -4, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.1; color[1] = 0.9; color[2] = 0.5;
	coord_q = { 4, -8, 5, 3, -2, 5,  3, -2, 0, 4, -8, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //

	color[0] = 0.9; color[1] = 0.1; color[2] = 0.9;
	coord_q = { 3, -2, 5,  8, 2, 5, 8, 2, 0,  3, -2, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.1; color[1] = 0.3; color[2] = 0.9;
	coord_q = { 8, 2, 5,  3, 1, 5,  3, 1, 0,  8, 2, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.9; color[1] = 0.9; color[2] = 0.1;
	coord_q = { 3, 1, 5,  1, 7, 5,  1, 7, 0, 3, 1, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.5; color[1] = 0.3; color[2] = 0.75;
	coord_q = { 1, 7, 5,  -6, 3, 5,  -6, 3, 0, 1, 7, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.1; color[1] = 0.8; color[2] = 0.9;
	coord_q = { -6, 3, 5,  -2, -5, 5,  -2, -5, 0,  -6, 3, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 1; color[1] = 1; color[2] = 0.7;
	coord_q = { -2, -5, 5,  2, -4, 5,  2, -4, 0, -2, -5, 0 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.8; color[1] = 0.3; color[2] = 0.5;
	coord_t = { 2, -4, 5, 3, -2, 5, 4, -8, 5 };
	fill_trian_mas(coord_t, trian_coord);
	draw_trian(color, trian_coord); //ok

	color[0] = 0.1; color[1] = 0.3; color[2] = 0.2;
	coord_t = { 3, -2, 5, 3, 1, 5, 8, 2, 5 };
	fill_trian_mas(coord_t, trian_coord);
	draw_trian(color, trian_coord); //ok

	color[0] = 0.5; color[1] = 0.2; color[2] = 0.2;
	coord_q = { 3, 1, 5, 3, -2, 5,  2, -4, 5,  -2, -5, 5 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok

	color[0] = 0.1; color[1] = 0.7; color[2] = 0.1;
	coord_q = { -6, 3, 5,  1, 7, 5,  3, 1, 5, -2, -5, 5 };
	fill_quad_mas(coord_q, quad_coord);
	draw_quad(color, quad_coord); //ok


}