#include<stdio.h>
#include<graphics.h>
#define WIN_WIDTH 900
#define WIN_HEIGHT 600
#include "tools.h"
#include<time.h>
#include<mmsystem.h>
#pragma comment(lib,"winmm.lib")
#include<math.h>
#include "vector2.h"
#define ZB_MAX 10

IMAGE bgImage;
IMAGE barImage;
enum{PEA,SUNFLOWER,PLANT_COUNT};
IMAGE plants[PLANT_COUNT];
int curX, curY;
IMAGE* dance[PLANT_COUNT][20];
int plant_index;
int score = 50;
IMAGE zbStand[11];
//游戏状态
enum{GOING,WIN,OVER};
int zb_kill;
int zb_gen;
int gameStatus = GOING;

struct plant
{
	int type;
	int frameIndex;
	bool catched;//是否处于被捕获状态
	int deadTime;//死亡持续时间
	int x, y;
	int timer;//留给向日葵用的
};

//使用贝塞尔曲线来生成阳光p1为起点 p2 p3为控制点 p4为重点 pCur为当前未知 pTime是时间 speed是速度
struct sunshineBall
{
	int x, y;//起始点的x,y坐标
	int frameIndex;//播放帧的序号
	int used;//是否被使用
	int time;

	float t;
	vector2 p1,p2,p3,p4;
	vector2 pCur;
	float speed;
	int status;
};

struct zb
{
	int x, y;
	int frameIndex;
	bool used;
	int speed;
	int row;//僵尸所在的行
	int health;//僵尸的血量值
	int dead;//是否处于死亡状态
	bool eating;//是否处于吃植物的状态
};

struct bullet
{
	bool used;//子弹是否被使用
	int x, y;//子弹的x,y坐标
	int row;//子弹在哪一行
	int speed;//子弹飞行的速度
	bool blast;//是否发生爆炸
	int frameIndex;//爆炸之后播放的序列帧的序号
};

struct bullet bullets[30];
//创建一个数组来存放僵尸
struct zb zbs[10];

//使用一个数组来存放僵尸的各个帧
IMAGE imgZB[22];
IMAGE sunshine[29];
//使用池子来存放阳光球
sunshineBall balls[10];
IMAGE imgBulletNormal;
IMAGE imgBulletBlast[4];
IMAGE imgZBDead[20];
IMAGE imgZBEating[21];

struct plant map[3][9];
enum{SUNSHINE_DOWN, SUNSHINE_GROUND,SUNSHINE_COLLECT,SUNSHINE_PRODUCT};

bool fileExist(const char* file)
{

	FILE* f = fopen(file, "r");
	if (f)
	{
		fclose(f);
	}

	return f != NULL;
}

void drawZB()
{
	//显示僵尸
	int zbMax = sizeof(zbs) / sizeof(zbs[0]);
	for (int i = 0; i < zbMax; i++)
	{
		if (zbs[i].used)
		{
			IMAGE* img = NULL;
			if (zbs[i].dead) {
				img = & imgZBDead[zbs[i].frameIndex];
			}
			else if (zbs[i].eating)
			{
				img = &imgZBEating[zbs[i].frameIndex];
			}
			else {
				img = &imgZB[zbs[i].frameIndex];
			}
			putimagePNG(zbs[i].x, zbs[i].y, img);
		}
	}
}

void generateSunshine()
{
	//避免创建的速度过快
	static int count = 0;
	//以一个随机的速度创建
	static int freq = 400;
	count++;
	if (count >= freq)
	{
		freq = 200 + rand() % 200;
		count = 0;
		//选出一个没有被使用的阳光
		int ballMax = sizeof(balls) / sizeof(balls[0]);
		int i;
		for (i = 0; i < ballMax && balls[i].used; i++);
		if (i >= ballMax) return;
		balls[i].used = true;
		balls[i].frameIndex = 0;

		balls[i].status = SUNSHINE_DOWN;
		int v = 250 + rand() % (900 - 250);
		balls[i].p1 = vector2(v, 60);
		balls[i].p4 = vector2(v, 180 + 100 * (rand() % 4));
		balls[i].t = 0;
		
		int off = 2;
		float distance = balls[i].p4.y - balls[i].p1.y;
		balls[i].speed = 1.0 / (distance / off);
	}

	//向日葵生产阳光
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			if (map[i][j].type == SUNFLOWER)
			{
				map[i][j].timer++;
				if (map[i][j].timer > 200)
				{
					map[i][j].timer = 0;
					int k;
					int maxBall = sizeof(balls) / sizeof(balls[0]);
					for (k = 0; k < maxBall&&balls[k].used; k++);
					if (k >= maxBall) return;

					//找到可用的阳光球
					balls[k].used = true;
					balls[k].p1 = vector2(map[i][j].x, map[i][j].y);
					int w = (100 + rand() % 100) * (rand() % 2 == 0 ? 1 : -1);
					balls[k].p4 = vector2(map[i][j].x + w, map[i][j].y + dance[SUNFLOWER][0]->getheight() -
						sunshine[0].getheight());
					balls[k].p2 = vector2(map[i][j].x + 0.3 * w, map[i][j].y - 100);
					balls[k].p3 = vector2(map[i][j].x + 0.7 * w, map[i][j].y - 100);
					balls[k].speed = 0.05;
					balls[k].t = 0;
					balls[k].status = SUNSHINE_PRODUCT;
				}
			}
		}
	}

}


void initGame()
{
	loadimage(&bgImage, "res/bg.jpg");
	loadimage(&barImage, "res/bar5.png");
	loadimage(&imgBulletNormal, "res/bullets/bullet_normal.png");
	char name[64];
	memset(dance, 0, sizeof(dance));
	zb_kill = 0;
	zb_gen = 0;

	for (int i = 0; i < PLANT_COUNT; i++)
	{
		//将植物卡牌加载到数组里
		sprintf_s(name, sizeof(name), "res/Cards/card_%d.png", i + 1);
		loadimage(&plants[i], name);

		for (int j = 0; j < 20; j++)
		{
			sprintf_s(name, sizeof(name), "res/zhiwu/%d/%d.png", i, j + 1);
			if (fileExist(name))
			{
				dance[i][j] = new IMAGE;
				loadimage(dance[i][j], name);
			}
			else {
				break;
			}
		}
	}
	//1代表显示控制台
	initgraph(WIN_WIDTH, WIN_HEIGHT,1);

	memset(map, 0, sizeof(map));
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			//-1代表此地没有植物;
			map[i][j].type = -1;
		}
	}

	memset(balls, 0, sizeof(balls));
	//配置随机数种子
	srand(time(NULL));
	//初始化阳光
	for (int i = 0; i < 29; i++)
	{
		sprintf_s(name, sizeof(name), "res/sunshine/%d.png", i + 1);
		loadimage(&sunshine[i], name);
	}


	//初始化僵尸
	for (int i = 0; i < 22; i++)
	{
		sprintf_s(name, sizeof(name), "res/zm/%d.png", i + 1);
		loadimage(&imgZB[i], name);
	}
	memset(zbs, 0, sizeof(zbs));
	//初始化子弹
	memset(bullets, 0, sizeof(bullets));

	//初始化爆炸图片
	loadimage(&imgBulletBlast[3], "res/bullets/bullet_blast.png");
	for (int i = 0; i < 3; i++)
	{
		float k = (i + 1) * 0.2;
		loadimage(&imgBulletBlast[i], "res/bullet_blast.png", imgBulletBlast[3].getwidth() * k,
			imgBulletBlast[3].getheight() * k, true);
	}

	//初始化死亡僵尸的图片
	for (int i = 0; i < 20; i++)
	{
		sprintf_s(name, sizeof(name), "res/zm_dead/%d.png", i + 1);
		loadimage(&imgZBDead[i], name);
	}

	//初始化僵尸吃植物的图片
	for (int i = 0; i < 21; i++)
	{
		sprintf_s(name, sizeof(name), "res/zm_eat/%d.png", i + 1);
		loadimage(&imgZBEating[i], name);
	}

	//加载站立僵尸的动画帧
	for (int i = 0; i < 11; i++)
	{
		sprintf_s(name, sizeof(name), "res/zm_stand/%d.png", i + 1);
		loadimage(&zbStand[i], name);
	}

}

//渲染植物
void drawPlant()
{
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			if (map[i][j].type != -1)
			{
				putimagePNG(map[i][j].x, map[i][j].y, dance[map[i][j].type][map[i][j].frameIndex]);
			}
		}
	}
	if (curX > 0 && curY > 0)
	{
		putimagePNG(curX - 65 / 2, curY - 90 / 2, dance[plant_index][0]);
	}
}

void drawSunshineBall()
{
	int ballMax = sizeof(balls) / sizeof(balls[0]);
	for (int i = 0; i < ballMax; i++)
	{
		if (balls[i].used)
		{
			putimagePNG(balls[i].pCur.x, balls[i].pCur.y, &sunshine[balls[i].frameIndex]);
		}
	}
}

void drawBullet()
{
	int bulletMax = sizeof(bullets) / sizeof(bullets[0]);
	for (int i = 0; i < bulletMax; i++)
	{
		if (bullets[i].used)
		{
			if (bullets[i].blast == true)
			{
				IMAGE* img = &imgBulletBlast[bullets[i].frameIndex];
				putimagePNG(bullets[i].x, bullets[i].y, img);
			}
			else {
				putimagePNG(bullets[i].x, bullets[i].y, &imgBulletNormal);
			}
		}
	}
}

//渲染
void updateWindow()
{
	BeginBatchDraw();
	putimage(0, 0, &bgImage);
	putimage(250, 0, &barImage);

	//顶部植物栏
	for (int i = 0; i < PLANT_COUNT; i++)
	{
		putimagePNG(338+65*i, 6, &plants[i]);
	}

	//渲染植物
	drawPlant();

	//渲染阳光球
	drawSunshineBall();

	//显示阳光值
	char ch[8];
	sprintf_s(ch, sizeof(ch), "%d", score);
	outtextxy(276,67,ch);

	//渲染僵尸
	drawZB();

	//显示子弹
	drawBullet();
	EndBatchDraw();
}

void collectSunshine(ExMessage* msg)
{
	int size = sizeof(balls) / sizeof(balls[0]);
	int w = sunshine[0].getwidth();
	int h = sunshine[0].getheight();


	for (int i = 0; i < size; i++)
	{
		if (balls[i].used&& msg->x > balls[i].pCur.x && msg->x<balls[i].pCur.x + w &&
			msg->y>balls[i].pCur.y && msg->y < balls[i].pCur.y + h)
		{
			balls[i].status = SUNSHINE_COLLECT;
			//balls[i].used = 0;
			mciSendString("play res/sunshine.mp3", 0, 0, 0);
			//PlaySound("res/sunshine.wav", NULL, SND_FILENAME| SND_SYNC);
			//score += 25;
			balls[i].p1 = balls[i].pCur;
			balls[i].p4 = vector2(262, 0);
			balls[i].t = 0;
			float distance = dis(balls[i].p1 - balls[i].p4);
			float off = 8;
			balls[i].speed = 1.0 / (distance / off);
		}
	}
}


void userClick()
{
	ExMessage msg;
	static int status=0;
	if (peekmessage(&msg))
	{
		if (msg.message == WM_LBUTTONDOWN)
		{
			//判断点击的是哪一个植物
			if (msg.x >= 338 && msg.x <= 338 + PLANT_COUNT * 65 &&
				msg.y >= 6 && msg.y <= 96)
			{
				status = 1;
				plant_index = (msg.x - 338) / 65;
			}
			else {
				collectSunshine(&msg);
			}
		}
		else if (status==1&&msg.message == WM_MOUSEMOVE)
		{
			curX = msg.x;
			curY = msg.y;

		}
		else if(msg.message==WM_LBUTTONUP){

			//先判断松开的位置是否合法
			if (curX > 250 && curX < 250 + 80 * 9 && curY>175 && curY < 175 + 312)
			{

				int row = (curY-175) / 104;
				int col = (curX -250 ) / 80;
				if (plant_index == SUNFLOWER&&score>=50)
				{
					score -= 50;
				}
				else if (plant_index == PEA&&score>=100)
				{
					score -= 100;
				}
				else {
					curX = 0;
					curY = 0;
					status = 0;
					return;
				}
				map[row][col].type = plant_index;
				map[row][col].frameIndex = 0;

				map[row][col].x = 250 + 80 * col;
				map[row][col].y = 175 + 104 * row;
	
				curX = 0;
				curY = 0;
				status = 0;
			}

		}
	}
}

void createZB()
{
	if (zb_gen > ZB_MAX)
		return;
	//控制创建僵尸的速度
	static int count = 0;
	static int freq = 400;
	count++;
	if (count >freq)
	{
		freq = 200+rand()%200;
		count = 0;

		//从僵尸池里选择一个没有使用过的僵尸
		int zbMax = sizeof(zbs) / sizeof(zbs[0]);
		int i;
		for (i = 0; i < zbMax && zbs[i].used; i++);
		
		if (i < zbMax)
		{
			zb_gen++;
			zbs[i].x = WIN_WIDTH;
			zbs[i].row = rand() % 3;
			zbs[i].y = 150 + (zbs[i].row) * 100;
			zbs[i].used = true;
			zbs[i].speed = 1;
			zbs[i].health = 100;
			zbs[i].dead = false;
			zbs[i].eating = false;
		}
	}
}

//更新阳光球的位置
void updateSunshine()
{
	int ballMax = sizeof(balls) / sizeof(balls[0]);
	for (int i = 0; i < ballMax; i++)
	{
		if (balls[i].used)
		{
			balls[i].frameIndex++;
			balls[i].frameIndex %= 29;

			//判断不同状态采用不同操作
			if (balls[i].status == SUNSHINE_DOWN)
			{
				balls[i].t += balls[i].speed;
				balls[i].pCur = balls[i].p1 + balls[i].t * (balls[i].p4 - balls[i].p1);
				if (balls[i].t > 1)
				{
					balls[i].status = SUNSHINE_GROUND;
				}
			}
			else if (balls[i].status == SUNSHINE_GROUND)
			{
				balls[i].time++;
				if (balls[i].time > 100)
				{
					balls[i].used = false;
					balls[i].time = 0;
				}
			}
			else if (balls[i].status == SUNSHINE_COLLECT)
			{
				balls[i].t += balls[i].speed;
				balls[i].pCur = balls[i].p1 + (balls[i].p4 - balls[i].p1) * balls[i].t;
				if (balls[i].t > 1)
				{
					balls[i].used = false;
					score += 25;
					balls[i].time = 0;
				}
			}
			else if (balls[i].status == SUNSHINE_PRODUCT)
			{
				balls[i].t += balls[i].speed;
				balls[i].pCur = calcBezierPoint(balls[i].t, balls[i].p1, balls[i].p2, balls[i].p3, balls[i].p4);
				if (balls[i].t > 1)
				{
					balls[i].status = SUNSHINE_GROUND;
				}
			}
		}
	}
}

void updateZB()
{
	static int count = 0;
	if (++count < 2) return;
	count = 0;
	//让僵尸行走起来
	int zbMax = sizeof(zbs) / sizeof(zbs[0]);
	for (int i = 0; i < zbMax; i++)
	{
		if (zbs[i].used)
		{
			zbs[i].x -= zbs[i].speed;
			if (zbs[i].dead)
			{
				zbs[i].frameIndex++;
				if (zbs[i].frameIndex >= 20)
				{
					zbs[i].used = false;
				}
			}
			else if (zbs[i].eating)
			{
				zbs[i].frameIndex = (zbs[i].frameIndex + 1) % 21;
			}
			else {
				zbs[i].frameIndex = (zbs[i].frameIndex + 1) % 22;
			}
			if (zbs[i].x < 230)
			{
				gameStatus = OVER;
				//printf("Game over!\n");
				//exit(0);
			}
		}
	}
}

void shoot()
{
	static int count = 0;
	if (++count < 2) return;
	count = 0;

	int line[3] = { 0};
	int zbMax = sizeof(zbs) / sizeof(zbs[0]);
	for (int i = 0; i < zbMax; i++)
	{
		if (zbs[i].used == true)
		{
			line[zbs[i].row] = 1;
		}
	}

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			//有豌豆并且有僵尸
			if (map[i][j].type == 0 && line[i] == 1)
			{
				//创建子弹
				int k;
				int bulleMax = sizeof(bullets) / sizeof(bullets[0]);
				for (k = 0; k < bulleMax && bullets[k].used; k++);
				if (k < bulleMax)
				{
					//delay
					static int count = 0;
					count++;
					if (count > 20)
					{
						count = 0;
						bullets[k].speed = 5;
						bullets[k].used = true;
						bullets[k].x = map[i][j].x + 65;
						bullets[k].y = map[i][j].y + 18;
						bullets[k].frameIndex = 0;
						bullets[k].blast = false;
						bullets[k].row = i;
					}
				}
			}
		}
	}
}

//使得子弹移动起来
void updateShoot()
{
	static int count = 0;
	if (++count < 2) return;
	count = 0;

	int bulletMax = sizeof(bullets) / sizeof(bullets[0]);
	for (int i = 0; i < bulletMax; i++)
	{
		if (bullets[i].used)
		{
			if (bullets[i].blast == true) bullets[i].frameIndex++;
			bullets[i].x += bullets[i].speed;

			if (bullets[i].x > WIN_WIDTH||bullets[i].frameIndex>=4)
			{
				bullets[i].used = false;
			}
		}
	}
}

void bullet2ZB()
{
	int bCount = sizeof(bullets) / sizeof(bullets[0]);
	int zCount = sizeof(zbs) / sizeof(zbs[0]);
	for (int i = 0; i < bCount; i++)
	{
		if (bullets[i].used == false) continue;
		for (int j = 0; j < zCount; j++)
		{
			if (zbs[j].used == false) continue;
			int x1 = zbs[j].x + 80;
			int x2 = zbs[j].x + 110;
			//如果子弹和僵尸在同一行并且x坐标位于x1和x2之间，即可认为发生碰撞
			if (zbs[j].dead==false&&!bullets[i].blast&&bullets[i].row == zbs[j].row && bullets[i].x > x1 && bullets[i].x < x2)
			{
				//子弹静止并且将状态设为爆炸状态
				bullets[i].speed = 0;
				bullets[i].blast = true;

				zbs[j].health -= 20;
				//printf("僵尸生命值：%d\n", zbs[j].health);
				if (zbs[j].health <= 0)
				{
					zb_kill++;
					if (zb_kill == ZB_MAX)
					{
						gameStatus = WIN;
					}
					//将僵尸设置为死亡状态，开始播放死亡相关的序列帧
					zbs[j].dead = true;
					zbs[j].frameIndex = 0;
					zbs[j].speed = 0;
				}
				break;
			}
		}
	}
}

void plant2ZB()
{
	int zbMax = sizeof(zbs) / sizeof(zbs[0]);
	for (int i = 0; i < zbMax; i++)
	{
		if (zbs[i].used)
		{
			//只判断僵尸所在的行是否有植物，一共有9列
			int row = zbs[i].row;
			for (int j = 0; j < 9; j++)
			{
				//判断是否有植物
				if (map[row][j].type != -1)
				{
					//碰撞检测
					int x1 = map[row][j].x+10;
					int x2 = map[row][j].x + 60;
					int x3 = zbs[i].x + 80;

					//检测到碰撞
					if (x3<x2 && x3>x1)
					{
						//判断是否是第一次碰撞
						if (map[row][j].catched&&zbs[i].eating==true)
						{
							map[row][j].deadTime++;
							if (map[row][j].deadTime > 200)
							{
								//植物消失 僵尸正常行走
								//memset(map, 0, sizeof(map));
								map[row][j].type = -1;
								map[row][j].catched = false;
								map[row][j].timer = 0;
								map[row][j].deadTime = 0;

								zbs[i].speed = 1;
								zbs[i].eating = false;
							}
						}
						else {
							map[row][j].catched = true;
							map[row][j].deadTime = 0;
							zbs[i].speed = 0;
							zbs[i].eating = true;
							zbs[i].frameIndex = 0;
						}
					}
				}

			}
		}
	}
}

//碰撞检测
void collisionCheck()
{
	bullet2ZB();
	plant2ZB();
}

void updatePlant()
{
	static int count = 0;
	if (++count < 3) return;
	count = 0;
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			//有植物
			if (map[i][j].type != -1)
			{
				map[i][j].frameIndex++;
				int index = map[i][j].type;
				int frame = map[i][j].frameIndex;
				if (dance[index][frame] == NULL)
				{
					map[i][j].frameIndex = 0;
				}
			}
		}
	}
}

void frameChange()
{

	//更新植物
	updatePlant();
	//创建阳光
	generateSunshine();
	updateSunshine();
	//创建僵尸
	createZB();
	updateZB();

	//创建子弹
	shoot();
	updateShoot();

	//碰撞检测
	collisionCheck();
};

void startMenu()
{
	IMAGE imgMenu,imgMenu1,imgMenu2;
	loadimage(&imgMenu, "res/menu.png");
	loadimage(&imgMenu1, "res/menu1.png");
	loadimage(&imgMenu2, "res/menu2.png");
	bool flag = false;
	while (1)
	{
		BeginBatchDraw();
		putimagePNG(0, 0, &imgMenu);
		putimagePNG(474, 75, flag ? &imgMenu1 : &imgMenu2);
		ExMessage msg;
		if (peekmessage(&msg))
		{
			if (msg.message == WM_LBUTTONDOWN&&msg.x>475&&msg.x<331+475
				&&msg.y>75&&msg.y<145+75)
			{
				flag = true;
			}
			else if (flag && msg.message == WM_LBUTTONUP)
			{
				return;
			}
		}
		EndBatchDraw();
	}
}

//片头巡场
void viewScene()
{
	vector2 points[9] = {
		{550,80},{530,160},{630,170},{530,200},{515,270},
		{565,370},{605,340},{705,280},{690,340}
	};
	int index[9];
	for (int i = 0; i < 9; i++)
	{
		index[i] = rand() % 11;
	}

	int xMin = -500;

	int count = 0;
	for (int i = 0; i > xMin; i -= 4)
	{
		BeginBatchDraw();
		//绘制背景图片
		putimagePNG(i, 0, &bgImage);
		//绘制僵尸
		count++;
		for (int k = 0; k < 9; k++)
		{
			putimagePNG(points[k].x-xMin+i, points[k].y, &zbStand[index[k]]);
			if(count>5)
				index[k] = (index[k] + 1) % 11;
		}
		if (count > 10)
			count = 0;
		EndBatchDraw();
		Sleep(5);
	}
	//停留
	for (int i = 0; i < 100; i++)
	{
		BeginBatchDraw();
		putimage(xMin, 0, &bgImage);
		for (int k = 0; k < 9; k++)
		{
			putimagePNG(points[k].x, points[k].y, &zbStand[index[k]]);
			index[k] = (index[k] + 1) % 11;
		}
		EndBatchDraw();
		Sleep(50);
	}

	for (int i = xMin; i <= 0; i += 2)
	{
		BeginBatchDraw();
		putimagePNG(i, 0, & bgImage);
		for (int k = 0; k < 9; k++)
		{
			putimagePNG(points[k].x-xMin+i,points[k].y,&zbStand[index[k]]);
			index[k] = (index[k] + 1) % 11;
		}
		EndBatchDraw();
		Sleep(5);
	}

}

//实现顶部植物栏的滑动
void showBar()
{
	int height = barImage.getheight();
	for (int y = -height; y <= 0; y++)
	{
		BeginBatchDraw();
		putimagePNG(250, y, &barImage);
		//顶部植物栏
		for (int i = 0; i < PLANT_COUNT; i++)
		{
			putimagePNG(338 + 65 * i, y+6, &plants[i]);
		}
		EndBatchDraw();
		Sleep(50);
	}
}

//判定游戏是否结束
bool checkGameStatus()
{
	bool res = false;
	if (gameStatus == WIN)
	{
		Sleep(2000);
		loadimage(0, "res/gameWin.png");
		mciSendString("play res/win.mp3", 0, 0, 0);
		res = true;
	}
	else if (gameStatus == OVER)
	{
		Sleep(2000);
		loadimage(0, "res/gameFail.png");
		mciSendString("play res/lose.mp3", 0, 0, 0);
		res = true;
	}
	return res;
}

int main(void)
{
	initGame();
	startMenu();
	int timer = 0;
	viewScene();
	showBar();
	while (1)
	{
		userClick();
		timer+=getDelay();
		if (timer > 10)
		{
			if (checkGameStatus())
			{
				break;
			}
			updateWindow();
			frameChange();
			timer = 0;
		}
	}
	system("pause");
	return 0;
}