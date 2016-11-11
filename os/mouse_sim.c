/***************************************************************************
 *   Copyright (C) 2004 by cyril dupuit                                    *
 *   cyrildupuit@hotmail.com                                               *
 *   http://perso.wanadoo.fr/koalys/                                       *
 *   (Adaptation for SOS by d2 -- 2004/12/20)                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

//*****************************************************************************
// Nom du module : MouseSim.c
// Description : Creation et destruction de souris mangeuse de fromages
//*****************************************************************************

#include <os/assert.h>
#include <lib/klibc.h>
#include <os/thread.h>
#include <os/ksynch.h>
#include <os/kmalloc.h>
#include <lib/x86_videomem.h>
 
// Historique :
// 20/12/04 : Suppr DestroyMap et suppr handler kbd dans version LM (d2)
// 26/11/04 : Bug trouve et resolu dans la fonction DestroyMap
// 21/11/04 : Creation du module V1.0

//*****************************************************************************
// Definition des equivalences :
//*****************************************************************************
#define MAP_X		76
#define MAP_Y		12
#define MAP_SIZE 	MAP_X * MAP_Y

#define MOUSE	0x01
#define CHEESE	0x02
#define OBSTACLE 0x04
#define INPUT	0x08
#define OUTPUT	0x10

#define OBSTACLE_COUNT	100
#define CHEESE_COUNT	650

#define MOUSE_FULL	0x01
#define MOUSE_EMPTY	0x02
#define CHEESE_FOUND	0x04
#define MOUSE_EXITED    0x08

#define MOUSE_SPEED_MAX	1000
#define MOUSE_SPEED_MIN 4

typedef unsigned int Color_t;

struct Point{
        int X;
        int Y;
        };

typedef struct Point Point_t;

#define Set(Reg, Flag)          Reg = (Reg | Flag)
#define Reset(Reg, Flag)        Reg = (Reg &(~Flag))
#define IsSet(Reg, Flag)        (Reg & Flag)


//*****************************************************************************
// Structure de gestion d'un element
//*****************************************************************************
struct Element{
	sos_ui32_t	        Type;//Type d'element
	sos_ui32_t	        Status;
	Color_t		        Color;//Couleur de l'element
	Point_t		        P;//Coordonnees de l'element
	struct sos_thread *	ThreadID;//Thread associe a la souris
	int		        Way;//Direction de la souris
	};

typedef struct Element Element_t;

//*****************************************************************************
// Prototypes des fonctions/procedures :
//*****************************************************************************
static void MouseCommander(void);
static void DrawMap(void);
static sos_ret_t CreateMap(void);
static sos_ret_t InitMapInput(Element_t * * pMap);
static sos_ret_t InitMapOutput(Element_t * * pMap);
static sos_ret_t ElementInit(Element_t * * pMap, unsigned int Type);
static void Mouse(unsigned long Param);
static void MouseMove(Point_t * P);
static Point_t ChoosePosition(Element_t * pMouse, int Positions[], int Count);
static int EvaluatePositions(Point_t Org, int Positions[], Point_t * Cheese);
static sos_bool_t IsCollision(Point_t Org, Point_t p, Point_t *Cheese);
static sos_bool_t AffectMovement(Point_t Org, Point_t p);
static void MouseCreator(void);
static sos_ret_t CreateMouse(void);

//*****************************************************************************
// Variables globales de ce module :
//*****************************************************************************

static Element_t * * pMap;
static struct sos_ksema SemMap;
static struct sos_ksema SemMouse;
static int MouseCount = 0;
static int CheeseCount = 0;
static int ObstacleCount = 0;
static int MouseSpeed = 100;

//*****************************************************************************
// Koalys Glue
//*****************************************************************************
void DrawPixel(int x, int y, Color_t color)
{
  os_putchar(y+3, x+2, color, 219);
}



//*****************************************************************************
// Point d'entre de la 'simulation'
//*****************************************************************************
void MouseSim(void)
{
  //Creation du semaphore de protection de la carte
  SOS_ASSERT_FATAL(SOS_OK == sos_ksema_init(& SemMap, "SemMap", 1));
	
  //Creation du semaphore de creation de souris
  SOS_ASSERT_FATAL(SOS_OK == sos_ksema_init(& SemMouse, "SemMouse", 2));
	
  //Creation de la carte
  SOS_ASSERT_FATAL(SOS_OK == CreateMap());

  //Creation du thread createur de souris
  SOS_ASSERT_FATAL(sos_create_kernel_thread("MouseCreator",
					    (sos_kernel_thread_start_routine_t)MouseCreator,
					    0) != NULL);

}


//*****************************************************************************
// But de la fonction : Creer et initialiser la carte
// Entree : Aucune
// Parametre retourne : ERROR si la memoire est insuffisante, TRUE sinon
//*****************************************************************************
static sos_ret_t CreateMap(void)
{
	pMap = (Element_t * *)sos_kmalloc(MAP_SIZE * sizeof(Element_t *), 0);
	if(pMap == NULL) return -SOS_ENOMEM;
	
	//Mettre la carte a 0
	memset(pMap, 0, MAP_SIZE * sizeof(Element_t *));
	
	//Initialisation de l'entree de la carte
	if(SOS_OK != InitMapInput(pMap))
	{//Memoire insuffisante
		return -SOS_EFATAL;
	}
	
	//Initialisation de la sortie de la carte
	if(InitMapOutput(pMap) != SOS_OK)
	{//Memoire insuffisante
		return -SOS_EFATAL;
	}
	
	//Initialisation du fromage
	if(ElementInit(pMap, CHEESE) != SOS_OK)
	{//Memoire insuffisante
		return -SOS_EFATAL;
	}
	
	//Initialisation des obstacles
	if(ElementInit(pMap, OBSTACLE) != SOS_OK)
	{//Memoire insuffisante
		return -SOS_EFATAL;
	}
	
	DrawMap();//Afficher la carte creee
	
	return SOS_OK;
}

//*****************************************************************************
// But de la procedure : Dessiner la carte a l'ecran
// Entree : Aucune
// Sortie : Aucune
//*****************************************************************************
static void DrawMap(void)
{
	unsigned int I;
	
	for(I = 0; I < MAP_SIZE; I++)
	{
		if(pMap[I] != NULL)
		{
			DrawPixel(I % MAP_X, I/MAP_X, pMap[I]->Color);
		}
		else DrawPixel(I % MAP_X, I/MAP_X, SOS_X86_VIDEO_FG_BLACK);
	}
	os_printf(23, 0, SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
				"Souris = %d; Fromages = %d; Obstacles = %d       ",
				MouseCount, CheeseCount, ObstacleCount);
}

//*****************************************************************************
// But de la fonction : Initialiser l'entree de la carte
// Entree :
//	pMap : Pointeur sur la carte
// Parametre retourne : ERROR si memoire insuffisante, TRUE sinon
//*****************************************************************************
static sos_ret_t InitMapInput(Element_t * * pMap)
{
	Element_t * pElement;
	
	//Definir le point d'entree
	pElement = (Element_t *)sos_kmalloc(sizeof(Element_t), 0);
	if(pElement == NULL) return -SOS_ENOMEM;
	
	//Initialiser l'entree
	pElement->Type = INPUT;
	pElement->Status = 0;
	pElement->Color = SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE;
	pElement->P.X = 0;
	pElement->P.Y = MAP_Y / 2;
	pElement->ThreadID = 0;

	pMap[(pElement->P.Y * MAP_X) + pElement->P.X] = pElement;

	return SOS_OK;
}

//*****************************************************************************
// But de la fonction : Initialiser la sortie de la carte
// Entree :
//	pMap : Pointeur sur la carte
// Parametre retourne : ERROR si memoire insuffisante, TRUE sinon
//*****************************************************************************
static sos_ret_t InitMapOutput(Element_t * * pMap)
{
	Element_t * pElement;
	
	//Definir le point de sortie
	pElement = (Element_t *)sos_kmalloc(sizeof(Element_t), 0);
	if(pElement == NULL) return -SOS_ENOMEM;
	
	//Initialiser l'entree
	pElement->Type = OUTPUT;
	pElement->Status = 0;
	pElement->Color = SOS_X86_VIDEO_FG_LTBLUE;
	pElement->P.X = MAP_X - 1;
	pElement->P.Y = MAP_Y / 2;
	pElement->ThreadID = 0;

	pMap[(pElement->P.Y * MAP_X) + pElement->P.X] = pElement;

	return SOS_OK;
}

//*****************************************************************************
// But de la fonction : Initialiser un type d'objet sur la carte
// Entree : 
//	pMap : Pointeur sur la carte
//	Type : Type d'objet a initialiser
// Parametre retourne : ERROR si memoire insuffisante, TRUE sinon
//*****************************************************************************
static sos_ret_t ElementInit(Element_t * * pMap, unsigned int Type)
{
	unsigned int I, J;
	unsigned int Max;
	Color_t Color;
	
	if(Type == CHEESE)
	{//Type d'element = fromage
		Max = CHEESE_COUNT;
		Color = SOS_X86_VIDEO_FG_YELLOW;
	}
	else if(Type == OBSTACLE)
	{//Type d'element = Obstacle
		Max = OBSTACLE_COUNT;
		Color = SOS_X86_VIDEO_FG_GREEN;
	}
	else
	{//Aucune autre type reconnu
		return -SOS_EINVAL;
	}
	
	for(I = 0; I < Max; I++)
	{//Tirer les fromages
		J = random();
		J += random();
		J %= MAP_SIZE;
		if(pMap[J] == NULL)
		{//Si l'emplacement est libre
			pMap[J] = (Element_t *)sos_kmalloc(sizeof(Element_t),
							   0);
			if(pMap[J] == NULL) return -SOS_ENOMEM;

			pMap[J]->Type = Type;
			//Initialiser l'element
			if(Type == CHEESE)
			{//Type d'element = fromage
				CheeseCount++;
			}
			else if(Type == OBSTACLE)
			{//Type d'element = Obstacle
				ObstacleCount++;
			}
			
			pMap[J]->Color = Color;
			pMap[J]->Status = 0;
			pMap[J]->Color = Color;
			pMap[J]->P.X = J % MAP_X;
			pMap[J]->P.Y = J / MAP_X;
			pMap[J]->ThreadID = 0;
		}
	}
	
	return SOS_OK;
}


//*****************************************************************************
// But du thread : Deplacer la souris sur la carte selon les regles etablies.
// Regles :
// - La souris doit se placer devant l'entree puis commence a recolter du
// fromage.
// - Des que la souris a ramasse un morceau de fromage, elle doit aller en
// entree de la carte afin de deposer sa recolte.
// - Si une souris a prouve sa recolte, une autre souris est creee.
// - Si une souris prend la sortie, elle est eliminee.
//*****************************************************************************
static void Mouse(unsigned long Param)
{
	Element_t * pMouse = (Element_t *)Param;
	Point_t P;
	
	SOS_ASSERT_FATAL(pMouse != NULL);
	
	//Position de depart de la souris
	P = pMouse->P;
	P = pMouse->P;
	
	while(1)
	{
	  int delay_ms;
	  struct sos_time delay;

	  //La souris doit se deplacer
	  sos_ksema_down(& SemMap, NULL);

	  MouseMove(&P);
	  
	  sos_ksema_up(& SemMap);

	  // Est-ce que la souris est sortie ?
	  if (IsSet(pMouse->Status, MOUSE_EXITED))
	    // Oui => on sort
	    break;

	  // Delai entre MOUSE_SPEED_MIN et MouseSpeed - 1
	  delay_ms = MOUSE_SPEED_MIN + (random() % MouseSpeed);
	  delay.sec     = delay_ms / 1000;
	  delay.nanosec = (delay_ms % 1000) * 1000000;
	  sos_thread_sleep(& delay);
	}

	// Libere la structure associee
	sos_kfree((sos_vaddr_t)pMouse);
}

//*****************************************************************************
// But de la procedure : Deplacer la souris de maniere aleatoire sur la carte
// Entrees :
//	P : Position courante de la souris
// Sorties :
//	P : Position suivante de la souris
//*****************************************************************************
static void MouseMove(Point_t * P)
{
	Point_t Org;
	Point_t p;
	Point_t Cheese;
	int Positions[8];
	int Count = 0;
	Element_t * pMouse;

	Org = *P;
	
	pMouse = pMap[Org.X + (Org.Y * MAP_X)];
	
	Count = EvaluatePositions(Org, Positions, &Cheese);

	if(Count == 0) return;

	p = Org;

	if(IsSet(pMouse->Status, CHEESE_FOUND))
	{//Prendre le fromage
		Reset(pMouse->Status, CHEESE_FOUND);
		p = Cheese;
	}
	else
	{//Choisir une position au hasard
		p = ChoosePosition(pMouse, Positions, Count);
	}
	if(AffectMovement(Org, p) == FALSE) return;
	//Deplacer la souris
	pMap[Org.X + (Org.Y * MAP_X)] = NULL;
	pMap[p.X + (p.Y * MAP_X)] = pMouse;
	pMouse->P = p;
	//Mettre a jour l'affichage
	DrawPixel(Org.X, Org.Y, SOS_X86_VIDEO_FG_BLACK);
	DrawPixel(p.X, p.Y, pMouse->Color);
	os_printf( 23,0, SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE, "Souris = %d; Fromages = %d; Obstacles = %d      ", MouseCount, CheeseCount, ObstacleCount);
	//Mettre a jour les coordonnees
	*P = p;
}

//*****************************************************************************
// But de la fonction : Choisir un mouvement
// Entree :
//	pMouse : Pointeur sur la souris
//	Positions : Tableau de position possible
//	Count :Nombre de positions valides
// Sortie : Aucune
// Parametre retourne : Position choisie
//*****************************************************************************
static Point_t ChoosePosition(Element_t * pMouse, int Positions[], int Count)
{
	int I, J;
	Point_t p;
	
	for(J = 0; J < Count; J++)
	{//Chercher dans le tableau si cette position est disponible
		I = Positions[J];
		if(I == pMouse->Way)
		{//Poursuivre ce sens d'avance
			p = pMouse->P;
			switch(I)
			{
				case 0:
					p.Y++;
					break;
				case 1:
					p.X++;
					p.Y++;
					break;
				case 2:
					p.X++;
					break;
				case 3:
					p.Y--;
					p.X++;
					break;
				case 4:
					p.Y--;
					break;
				case 5:
					p.Y--;
					p.X--;
					break;
				case 6:
					p.X--;
					break;
				case 7:
					p.X--;
					p.Y++;
					break;
			}
			return p;
		}
	}
	
	J = random() % Count;
	I = Positions[J];
	if(((I + 4) % 8) == pMouse->Way)
	{//Eviter le sens inverse
		J = (J + 1) % Count;
		I = Positions[J];
	}
	
	p = pMouse->P;
	switch(I)
	{//Repere le deplacement
		case 0:
			p.Y++;
			break;
		case 1:
			p.X++;
			p.Y++;
			break;
		case 2:
			p.X++;
			break;
		case 3:
			p.Y--;
			p.X++;
			break;
		case 4:
			p.Y--;
			break;
		case 5:
			p.Y--;
			p.X--;
			break;
		case 6:
			p.X--;
			break;
		case 7:
			p.X--;
			p.Y++;
			break;
	}
	
	pMouse->Way = I;//Memoriser la direction selectionnee
	
	return p;
}

//*****************************************************************************
// But de la fonction : Evaluer les positions possibles et les memoriser dans
// un tableau de positions si aucun fromage n'a ete detecte. Si du fromage a
// ete detecte, il sera selectionne en premier. La presence d'un fromage est
// indiquee par le drapeau CHEESE_FOUND
// Entree :
//	Org : Position de la souris
// Sorties :
//	Positions : Tableau de positions valides
//	Cheese : Position du fromage
// Parametre retourne : Nombre de positions valides
//*****************************************************************************
static int EvaluatePositions(Point_t Org, int Positions[], Point_t * Cheese)
{
	int I;
	int Count = 0;
	Point_t p;
	Point_t CheesePos;
	
	for(I = 0; I < 8; I++)
	{//Explorer toute les directions
		p = Org;
		switch(I)
		{//Repere le deplacement
			case 0:
				p.Y++;
				break;
			case 1:
				p.X++;
				p.Y++;
				break;
			case 2:
				p.X++;
				break;
			case 3:
				p.Y--;
				p.X++;
				break;
			case 4:
				p.Y--;
				break;
			case 5:
				p.Y--;
				p.X--;
				break;
			case 6:
				p.X--;
				break;
			case 7:
				p.X--;
				p.Y++;
				break;
		}
		//Tester la collision
		if(IsCollision(Org, p, &CheesePos) == FALSE)
		{//La souris n'a rencontre aucun obstacle
			Positions[Count] = I;
			Count++;
		}
	}
	
	*Cheese = CheesePos;

	return Count;
}

//*****************************************************************************
// But de la fonction : Affecter un mouvement a la souris
// Entrees :
//	Org : Coordonnees de la souris
//	p : Coordonnees voulu par la souris
// Parametre retourne : TRUE si le mouvement a eu lieu, FALSE sinon
//*****************************************************************************
static sos_bool_t AffectMovement(Point_t Org, Point_t p)
{
	Element_t * pMouse = pMap[Org.X + (Org.Y * MAP_X)];
	Element_t * pElement;
	
	pElement = pMap[p.X + (p.Y * MAP_X)];
	
	//La place est libre
	if(pElement == NULL) return TRUE;//Autoriser le mouvement

	switch(pElement->Type)
	{
		case CHEESE:
			// Liberer l'emplacement memoire du fromage
			sos_kfree((sos_vaddr_t)pElement);
			pMap[p.X + (p.Y * MAP_X)] = NULL;
			
			//Donner le fromage a la souris
			Set(pMouse->Status, MOUSE_FULL);
			Reset(pMouse->Status, MOUSE_EMPTY);
			pMouse->Color = SOS_X86_VIDEO_FG_MAGENTA;
			CheeseCount--;
			return TRUE;
		case OUTPUT:
			//Supprimer la souris
			pMap[Org.X + (Org.Y * MAP_X)] = NULL;
			MouseCount--;
			DrawPixel(Org.X, Org.Y, SOS_X86_VIDEO_FG_BLACK);
			os_printf( 23,0, SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
						 "Souris = %d; Fromages = %d; Obstacles = %d       ",
						 MouseCount, CheeseCount,
						 ObstacleCount);
			Set(pMouse->Status, MOUSE_EXITED);
			return FALSE;
		default :
			return FALSE;
	}
	
	return FALSE;
}

//*****************************************************************************
// But de la fonction : Tester si une collision a eu lieu avec un obstacle
// Entrees :
//	Org : Coordonnees de la souris
//	p : coordonnees desirees par la souris
// Sortie :
//	Cheese : Coordonnees du fromage
// Parametre retourne : TRUE si une collision a eu lieu, FALSE sinon
//*****************************************************************************
static sos_bool_t IsCollision(Point_t Org, Point_t p, Point_t *Cheese)
{
	Element_t * pMouse = pMap[Org.X + (Org.Y * MAP_X)];
	Element_t * pElement;
	
	//Tester les bordures de la map
	if((p.X < 0)||(p.Y < 0)) return TRUE;
	
	if((p.Y >= MAP_Y)||(p.X >= MAP_X)) return TRUE;
	
	pElement = pMap[p.X + (p.Y * MAP_X)];
	
	//L'element est vide
	if(pElement == NULL) return FALSE;

	//Si du fromage a ete trouve, stopper la recherche
	if(IsSet(pMouse->Status, CHEESE_FOUND)) return FALSE;
	
	switch(pElement->Type)
	{
		case CHEESE:
			if(IsSet(pMouse->Status, MOUSE_FULL)) return TRUE;
			//Indiquer que du fromage a ete trouve
			Set(pMouse->Status, CHEESE_FOUND);
			//Retenir la position du fromage
			(*Cheese).X = p.X;
			(*Cheese).Y = p.Y;
			break;
		case INPUT:
			if(IsSet(pMouse->Status, MOUSE_EMPTY)) return TRUE;
			//Remplir les reserves de fromage
			Set(pMouse->Status, MOUSE_EMPTY);
			Reset(pMouse->Status, MOUSE_FULL);
			pMouse->Color = SOS_X86_VIDEO_FG_LTRED;
			//Autoriser la creation d'une autre souris
			sos_ksema_up(& SemMouse);
			return TRUE;
		case OUTPUT:
			break;
		default :
			return TRUE;
	}
	
	return FALSE;//Aucune collision
}

//*****************************************************************************
// But du thread : Creer une souris et la placer autour de l'entree
//*****************************************************************************
static void MouseCreator(void)
{	
	while(1)
	{
		sos_ksema_down(& SemMouse, NULL);
		sos_ksema_down(& SemMap, NULL);
		CreateMouse();
		sos_ksema_up(& SemMap);
	}
}

//*****************************************************************************
// But de la fonction : Creer une souris et l'inserer dans la carte
// Entree : Aucune
// Parametre retourne : ERROR si memoire insuffisante, FALSE si souris non
// cree, TRUE sinon
//*****************************************************************************
static sos_ret_t CreateMouse(void)
{
	Element_t * pElement;
	unsigned int I;

	Point_t p;

	for(I = 0; I < 8; I++)
	{//Explorer tous les emplacements
		p.X = 0;
		p.Y = MAP_Y / 2;
		switch(I)
		{//Repere le deplacement
			case 0:
				p.Y++;
				break;
			case 1:
				p.X++;
				p.Y++;
				break;
			case 2:
				p.X++;
				break;
			case 3:
				p.Y--;
				p.X++;
				break;
			case 4:
				p.Y--;
				break;
			case 5:
				p.Y--;
				p.X--;
				break;
			case 6:
				p.X--;
				break;
			case 7:
				p.X--;
				p.Y++;
				break;
		}
		if((p.X >= 0)&&(p.Y >= 0)&&(p.X < MAP_X)&&(p.Y < MAP_Y))
		{//L'emplacement est valide
			pElement = pMap[p.X + (p.Y * MAP_X)];
			if(pElement == NULL)
			{//Creer la souris
				pElement = (Element_t *)sos_kmalloc(sizeof(Element_t), 0);
				if(pElement != NULL)
				{//Initialiser l'entree
					pElement->Type = MOUSE;
					Set(pElement->Status, MOUSE_EMPTY);
					pElement->Color = SOS_X86_VIDEO_FG_LTRED;
					pElement->P = p;
					pElement->Way = 0;
					pElement->ThreadID
					  = sos_create_kernel_thread("Mouse",
								     (sos_kernel_thread_start_routine_t)Mouse,
								     pElement);
					if(pElement->ThreadID == 0)
					{
						sos_kfree((sos_vaddr_t)pElement);
						pElement = NULL;
						return -SOS_ENOMEM;
					}
					pMap[p.X + (p.Y * MAP_X)] = pElement;
					MouseCount++;

					DrawPixel(p.X, p.Y, pElement->Color);
					os_printf(23, 0, SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE, "Souris = %d; Fromages = %d; Obstacles = %d       ", MouseCount, CheeseCount, ObstacleCount);

					return SOS_OK;
				}
			}
		}
	}
	return -SOS_EBUSY;
}

//*****************************************************************************
// C'est fini !!!!
//*****************************************************************************
