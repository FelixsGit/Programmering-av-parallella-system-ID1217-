#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>

#define DEFAULTBODIES 100
#define DT 1
#define DEFAULTTIMESTEPS 1000
#define DEFAULTFAR 10
const double G = 0.0000000000667;

void calculateForces();
void initBodies();
void* work();
void moveBodies();
void start_clock(void);
void end_clock();
void insertIntoTree();
void initChildren();
void setCenterOfMasses();
void freeTree();
static clock_t st_time;
static clock_t en_time;
static struct tms st_cpu;
static struct tms en_cpu;
int numberOfBodies;
int numberOfTimesteps;
double far;
double spaceSize = 1000;

typedef struct vector{
  double x;
  double y;
}vector;

typedef struct body{
    struct vector pos, vel, force;
    double mass;
}body;

body* bodies;

typedef struct Node{
  struct body bodyInNode;
  bool isLeaf;
  bool hasParticle;
  struct vector pos;
  double size;
  double totalMass;
  struct Node* nw;
  struct Node* ne;
  struct Node* sw;
  struct Node* se;
  struct vector centerOfMass;
}Node;

Node *root;

Node* findQuadrant(vector pos, Node* parent){
  if(pos.x >= parent->pos.x + parent->size/2){
   if(pos.y >= parent->pos.y + parent->size/2){
     //printf("\nParticle with pos {%lf, %lf} was found in NE",pos.x,pos.y);
     return parent->ne;
   }
   else{
     //printf("\nParticle with pos {%lf, %lf} was found in SE",pos.x,pos.y);
     return parent->se;
   }
  }
  else{
   if(pos.y >= parent->pos.y + parent->size/2){
     //printf("\nParticle with pos {%lf, %lf} was found in NW",pos.x,pos.y);
     return parent->nw;
   }
   else{
     //printf("\nParticle with pos {%lf, %lf} was found in SW",pos.x,pos.y);
     return parent->sw;
   }
 }
}

void insertIntoTree(body particle, Node* node){
  node->totalMass += particle.mass;
  if(node->isLeaf && !node->hasParticle){
    node->bodyInNode = particle;
    node->hasParticle = true;
  }
  else if(!node->isLeaf && node->hasParticle){
    Node* newNodeToGoTo = findQuadrant(particle.pos, node);
    insertIntoTree(particle, newNodeToGoTo);
  }
  else if(node->isLeaf && node->hasParticle){
    if((node->bodyInNode.pos.x == particle.pos.x) && (node->bodyInNode.pos.y == particle.pos.y)){
      printf("\nTwo bodies in the same position --> CRASH\n");
      exit(1);
    }
    node->isLeaf = false;
    initChildren(node);
    Node* newNodeToGoToOldBody = findQuadrant(node->bodyInNode.pos, node);
    insertIntoTree(node->bodyInNode, newNodeToGoToOldBody);
    Node* newNodeToGoToNewBody = findQuadrant(particle.pos, node);
    insertIntoTree(particle, newNodeToGoToNewBody);
  }
}

void initChildren(Node* parent){
  parent->nw = (Node*)malloc(sizeof(Node));
  parent->ne = (Node*)malloc(sizeof(Node));
  parent->sw = (Node*)malloc(sizeof(Node));
  parent->se = (Node*)malloc(sizeof(Node));

  double sizeOfChildren = parent->size/2;
  parent->nw->size = parent->ne->size = parent->sw->size = parent->se->size = sizeOfChildren;

  parent->nw->pos.x = parent->pos.x;
  parent->nw->pos.y = parent->pos.y + sizeOfChildren;
  parent->nw->isLeaf = true;
  parent->nw->hasParticle = false;
  parent->nw->totalMass = 0.0;

  parent->ne->pos.x = parent->pos.x + sizeOfChildren;
  parent->ne->pos.y = parent->pos.y + sizeOfChildren;
  parent->ne->isLeaf = true;
  parent->ne->hasParticle = false;
  parent->ne->totalMass = 0.0;

  parent->sw->pos.x = parent->pos.x;
  parent->sw->pos.y = parent->pos.y;
  parent->sw->isLeaf = true;
  parent->sw->hasParticle = false;
  parent->sw->totalMass = 0.0;

  parent->se->pos.x = parent->pos.x + sizeOfChildren;
  parent->se->pos.y = parent->pos.y;
  parent->se->isLeaf = true;
  parent->se->hasParticle = false;
  parent->se->totalMass = 0.0;
}

void summarizeTree() {
  setCenterOfMasses(root);
}

vector ZERO_VECTOR(){
  struct vector v;
  v.x = 0;
  v.y = 0;
  return v;
}
vector calcNumeratorCOM(struct Node* n){
  struct vector v;
  if(n->isLeaf && !n->hasParticle){
    v = ZERO_VECTOR();
  }
  else if(n->isLeaf && n->hasParticle){
    v.x = n->bodyInNode.mass * n->bodyInNode.pos.x;
    v.y = n->bodyInNode.mass * n->bodyInNode.pos.y;
  }
  else{
    struct vector v1, v2, v3, v4;
    v1 = calcNumeratorCOM(n->nw);
    v2 = calcNumeratorCOM(n->ne);
    v3 = calcNumeratorCOM(n->sw);
    v4 = calcNumeratorCOM(n->se);
    v.x = v1.x + v2.x + v3.x + v4.x;
    v.y = v1.y + v2.y + v3.y + v4.y;
  }
  return v;
}

void setCenterOfMasses(struct Node* n) {
  if(n->isLeaf){
    return;
  }
  struct vector v = calcNumeratorCOM(n);
  struct vector res = ZERO_VECTOR();
  res.x = v.x / n->totalMass;
  res.y = v.y / n->totalMass;
  n->centerOfMass = res;

  setCenterOfMasses(n->nw);
  setCenterOfMasses(n->ne);
  setCenterOfMasses(n->sw);
  setCenterOfMasses(n->se);
}

void freeTree(struct Node* n) {
  if(!n->isLeaf) {
    freeTree(n->nw);
    freeTree(n->ne);
    freeTree(n->sw);
    freeTree(n->se);
  }
  free(n);
}

int main(int argc, char *argv[]) {
  numberOfBodies = ((argc > 1)? atoi(argv[1]) : DEFAULTBODIES);
  numberOfTimesteps = ((argc > 2)? atoi(argv[2]) : DEFAULTTIMESTEPS);
  far = ((argc > 3)? atoi(argv[3]) : DEFAULTFAR);

  initBodies();

  start_clock();
  for(int j = 0; j < numberOfTimesteps; j++){
    root = (Node*)malloc(sizeof(Node));
    root->size = spaceSize;
    root->pos.x = 0;
    root->pos.y = 0;
    root->isLeaf = true;
    root->totalMass = 0.0;
    root->hasParticle = false;

    for(int i = 0; i < numberOfBodies; i++){
      insertIntoTree(bodies[i], root);
    }
    summarizeTree();
    for(int i = 0; i < numberOfBodies; i++){
      calculateForces(i, root);
    }

    for(int i = 0; i < numberOfBodies; i++){
      moveBodies(i);
    }
    freeTree(root);
  }
  end_clock();
  for(int i = 0; i <numberOfBodies; i++){
    printf("\nVelocity of body %d is {%lf, %lf}", i + 1, bodies[i].vel.x, bodies[i].vel.y);
  }
}

void initBodies(){
  bodies = (body*)malloc(sizeof(body) * numberOfBodies);
  for(int i = 0; i < numberOfBodies; i++){
    time_t t;
    srand((unsigned) time(&t) * (i + 1));
    bodies[i].pos.x = rand()%1000;
    bodies[i].pos.y = rand()%1000;
    bodies[i].mass = rand()%900000000 + 100000000;
  }
}

void calculateForces(int currentBody, struct Node* currentNode){
  double distance;
  double magnitude;
  vector directions;
  if(currentNode->isLeaf && !currentNode->hasParticle){
    bodies[currentBody].force.x += 0;
    bodies[currentBody].force.y += 0;
  }else if(currentNode->isLeaf && currentNode->hasParticle){
    distance = sqrt(((bodies[currentBody].pos.x - currentNode->bodyInNode.pos.x) * (bodies[currentBody].pos.x - currentNode->bodyInNode.pos.x)) +
    ((bodies[currentBody].pos.y - currentNode->bodyInNode.pos.y) * (bodies[currentBody].pos.y - currentNode->bodyInNode.pos.y)));
    if(distance < 0.00010 ){
      distance = 0.00010;
    }
    magnitude = G * ((bodies[currentBody].mass * currentNode->bodyInNode.mass) / (distance * distance));
    directions.x = currentNode->bodyInNode.pos.x - bodies[currentBody].pos.x;
    directions.y = currentNode->bodyInNode.pos.y - bodies[currentBody].pos.y;
    bodies[currentBody].force.x += (magnitude * (directions.x / distance));
    bodies[currentBody].force.y += (magnitude * (directions.y / distance));

  }else if(!currentNode->isLeaf && currentNode->hasParticle){
    distance = sqrt(((bodies[currentBody].pos.x - currentNode->centerOfMass.x) * (bodies[currentBody].pos.x - currentNode->centerOfMass.y)) +
    ((bodies[currentBody].pos.y - currentNode->centerOfMass.y) * (bodies[currentBody].pos.y - currentNode->centerOfMass.y)));
    if(distance < 1 ){
      distance = 1;
    }
    if(distance > far){
      magnitude = G * ((bodies[currentBody].mass * currentNode->totalMass) / (distance * distance));
      directions.x = currentNode->centerOfMass.x - bodies[currentBody].pos.x;
      directions.y = currentNode->centerOfMass.y - bodies[currentBody].pos.y;
      bodies[currentBody].force.x +=  (magnitude * (directions.x / distance));
      bodies[currentBody].force.y +=  (magnitude * (directions.y / distance));
    }else{
      calculateForces(currentBody, currentNode->nw);
      calculateForces(currentBody, currentNode->ne);
      calculateForces(currentBody, currentNode->sw);
      calculateForces(currentBody, currentNode->se);
    }
  }
}

void moveBodies(int currentBody) {
  vector deltap;
  vector deltav;
  deltav.x = (bodies[currentBody].force.x / bodies[currentBody].mass) * DT;
  deltav.y = (bodies[currentBody].force.y / bodies[currentBody].mass) * DT;
  deltap.x = (bodies[currentBody].vel.x + deltav.x / 2) * DT,
  deltap.y = (bodies[currentBody].vel.y + deltav.y / 2) * DT,
  bodies[currentBody].vel.x = bodies[currentBody].vel.x + deltav.x;
  bodies[currentBody].vel.y = bodies[currentBody].vel.y + deltav.y;
  bodies[currentBody].pos.x = bodies[currentBody].pos.x + deltap.x;
  bodies[currentBody].pos.y = bodies[currentBody].pos.y + deltap.y;
  if(bodies[currentBody].pos.x > spaceSize || bodies[currentBody].pos.x < 0){
    bodies[currentBody].vel.x = bodies[currentBody].vel.x * -1;
  }
  if(bodies[currentBody].pos.y > spaceSize || bodies[currentBody].pos.y < 0){
    bodies[currentBody].vel.y = bodies[currentBody].vel.y * -1;
  }
  bodies[currentBody].force.x = bodies[currentBody].force.y = 0.0;
}

void start_clock(){
    st_time = times(&st_cpu);
}

void end_clock(){
    en_time = times(&en_cpu);
    printf("\nReal Time: %jd, User Time %jd, System Time %jd\n",
        (intmax_t)(en_time - st_time),
        (intmax_t)(en_cpu.tms_utime - st_cpu.tms_utime),
        (intmax_t)(en_cpu.tms_stime - st_cpu.tms_stime));
}
