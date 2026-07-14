#include <stdio.h>
#include "parser.h"
//parser permite convertir la cadena de texto argv en una estructura de datos tLine
//tCommand representa solo un comando de todos los que hay
//los redirect guardan los nomrbes de los ficheros si hay << >> <
//background es una variable, que nos detecta &
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>


#define MAXARG 64
#define MAXLINES 1024
#define MAXJB 12

typedef struct{
	pid_t pids[MAXARG];
	int n_pids;
	char cmd[MAXLINES];
	int active; //valor como booleano, para saber si este elemento esta ocupado
	unsigned long orden;
}tLista;

tLista *listaJobs;
unsigned long contadorJobs = 0;

void insertar(pid_t *pids, int n_pids, char *cmd);

void mycd(tcommand cmd); 

void myumask(tcommand cmd);

void myjobs(); 

void myfg(tcommand cmd);

void myexec(tline *line, char *cmd);

void limpiarZombies();

int buscarUltimoJob();



int main(int argc, char *argv[]){

	char buffer[1024]; //buffer para guardar lo que se escriba en el teclado
	tline *line; //declaramos la estructura de datos de los argumentos
	listaJobs = (tLista*) calloc (MAXJB, sizeof(tLista)); //asigancio dinamica a la estrucytura de datos
	if(listaJobs == NULL){
		perror("fallo de memoria"); //si diera error de memoria
		return -1;
	}
	
	if(argc > 1) { //en el caso que se escriba de una, directamente con el programa
		line = tokenize(argv[1]); //convertimos el texto en argumento en la estructura de datos
		if(line != NULL){
			//Entramos en la estructura linea -> al numero de comandos, despues accedemos a al primer arg y lo comparamos con cd y ejecutamos, es igual con el resto
			if(line -> ncommands == 1 && strcmp(line -> commands[0].argv[0], "cd") == 0){
				mycd(line -> commands[0]);
					
			}else if(strcmp(line -> commands[0].argv[0], "umask") == 0){

			myumask(line -> commands[0]);	
		
			}else if(strcmp(line -> commands[0].argv[0], "jobs") == 0){

				myjobs();	
		
			}else if(strcmp(line -> commands[0].argv[0], "fg") == 0){

				myfg(line -> commands[0]);	
		
			}else{
				myexec(line, argv[1]);
			}
			}
			free(listaJobs);
		return 0;
	
	}

	signal(SIGINT, SIG_IGN); //si recibimos SIGINT == Control + c, decimos al S.O que lo ignore (SIG_IGN)
	

	while(1){ //creamos un bucle infinito
	
		limpiarZombies();
		printf("msh> "); //imprimos el promt
		fflush(stdout); //forzamos porque no tenemos \n
		
		if(fgets(buffer , 1024, stdin) == NULL){ //leemos lo que escriba el usuario
			printf("\n");
			break;
		}
		buffer[strcspn(buffer, "\n")] = 0;
    
		line = tokenize(buffer);//convertimos lo que recibamos en la ed
	
		if(line == NULL || line -> ncommands == 0 ) continue; //si no hay comandos o la linea es null volvemos a empezar
		
		
		if(strcmp(line -> commands[0].argv[0], "exit") == 0){ //mismo procedimiento de arriba
			break;
			
		}else if(strcmp(line -> commands[0].argv[0], "cd") == 0){

			mycd(line -> commands[0]);	
	
		}else if(strcmp(line -> commands[0].argv[0], "umask") == 0){

			myumask(line -> commands[0]);	
	
		}else if(strcmp(line -> commands[0].argv[0], "jobs") == 0){

			myjobs();	
	
		}else if(strcmp(line -> commands[0].argv[0], "fg") == 0){

			myfg(line -> commands[0]);	
	
		}else{
			myexec(line, buffer);
		}
	}


	free(listaJobs);



return 0;
}

void mycd(tcommand cmd){ 
	char *dir; //nuestro puntero guardara la ruta a la que queremos ir
	
	if(cmd.argc < 2){
		//si los argumenos son < 2 indica que solo esta escrito cd 
		dir = getenv("HOME"); //buscamos con getenv la variable de retorno de HOME y la guardamos en dir
		if(dir == NULL){ // si no la encontramos devolvemos un error
			fprintf(stderr, "Error: no existe la variable HOME\n");
			return;
			
		}
		
	}else{
		dir = cmd.argv[1]; //si ha puesto mas, apuntamos y guardamos el resto de variables
	}
	
	if(chdir(dir) != 0) perror("error en el cd");
	//cambianmos el directorio actual, si devuelve 0 es que ha ocurrido un error

}
 
void myexec(tline *line, char *cmd){ 
	int i;
	int pipefd[2]; //0 -> lectura; 1 -> escritura
	int fd_entrada = 0; //guardara el descriptor de lectura del hijo anterior
	pid_t pid; //id de los padres e hijos 
	int fd; //descriptor para los archivos
	pid_t *pids;

	pids = (pid_t *) malloc(line -> ncommands * sizeof(pid_t));

	if(pids == NULL){
		perror("Error de memoria en pids");
		return;
	}


	for (i = 0; i < line -> ncommands; i++){ //este for, lo que hara es crear hijos por cada comando que haya

		if(i < line -> ncommands - 1){ //crearemos un pipe con la siguiente ecuacion pipe = numero de comandos - 1, pues la salida del ultimo comando sera hacia la pantalla externa
				if(pipe(pipefd) < 0){//creamos un tubo
					perror("msh: error al crear el pipe"); //en caso de error
					if(fd_entrada != 0){
						close(fd_entrada);
						free(pids);
						return;

					}
				}

		}

		pid = fork(); //creamos el hijo

		if(pid < 0){
			perror("msh: error al crear el hijo");
			if(i < line -> ncommands - 1){
				close(pipefd[0]);
				close(pipefd[1]);

			}
			if(fd_entrada != 0){
				close(fd_entrada);

			}

			free(pids);
		}

		if(pid > 0){
			pids[i] = pid;
		}



		if(pid == 0){
			if(line -> background == 0){
				signal(SIGINT, SIG_DFL); //en el caso que el hijo no este en bg, restauramos la señar para que pueda ser interrupido con control + c
			}
			

			if(i == 0 && line -> redirect_input != NULL){ //en el caso que en el primer comando y hay redireccion de entrada <
				fd = open(line -> redirect_input, O_RDONLY); //abrimos el fichero 
				if(fd < 0){
					fprintf(stderr, "msh: %s: no existe fichero o directorio\n", line -> redirect_input);
					exit(1);
				}
				dup2(fd, STDIN_FILENO); //
				close(fd); //cerramos 
			}else if(fd_entrada != 0){ //tal caso que no sea el primero, y haya un pipe por hacer
				dup2(fd_entrada, STDIN_FILENO); //stdin es el pipe anterior
				close(fd_entrada);
			}
			if(i == line -> ncommands - 1 && line -> redirect_output != NULL){ //es el ultimo comando y hay redireccion de salida >
				fd = open(line -> redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if(fd < 0){
					fprintf(stderr, "msh: error al abrir el fichero %s\n", line -> redirect_output);
					exit(1);
				}
				dup2(fd, STDOUT_FILENO); //stdout es el fichero
				close(fd);
			}else if(i < line -> ncommands - 1){ //tal caso que no sea el ultimo comando y deba escribir en el pipe para el siguiente 
				dup2(pipefd[1], STDOUT_FILENO); //stdout es el fichero ahora 
				close(pipefd[1]);
				close(pipefd[0]);

			}

			if(line -> redirect_error != NULL){ //gestionamos el error 
				fd = open(line -> redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if(fd < 0){
					fprintf(stderr, "msh: error al abrir el fichero de error %s\n", line -> redirect_error);
					exit(1);
				}
				dup2(fd, STDERR_FILENO); //stderr es el fichero 
				close(fd);
			}

			execvp(line -> commands[i].argv[0], line -> commands[i].argv); //convertirmos el hijo en el programa 

			 // si retorna hubo error
			if(errno == ENOENT){
				fprintf(stderr, "%s: mandato no encontrado\n", line -> commands[i].argv[0]);
				
			}

			 exit(1);
		}


		if(fd_entrada != 0){ //si teniamos una entrada abierta la cerramos 
			close(fd_entrada);
		}

		if(i < line -> ncommands - 1){ //si acabamos de crear el pipe, no es el ultimo comando todavia
			close(pipefd[1]); //nunca vamos a escribir
			fd_entrada = pipefd[0]; //guardamos la lectura para el sigueitne for
		}
		
	}

	if(line -> background == 0){ // si no hay bg esperamos a todos los hijos 
		for(i = 0; i < line -> ncommands; i++){
			waitpid(pids[i], NULL, 0);
		}
	}else{
		printf("comando en background con PID: %d\n", pid); // si no no esperamos 
		insertar(pids, line -> ncommands, cmd);

	}

	free(pids);

}

void myumask(tcommand cmd){
	mode_t maskara, otra; //variables para guardar la mascara

	if(cmd.argc == 1){ //si solo queremos ver la mascara actual
		maskara = umask(0); //llamamos a umask para que nos devuelva el mismo valor
		umask(maskara); //y lo volvemos a poner
		printf("%04o\n", maskara); //como ya esta obtenido lo imprimimos 

	}else{ //si queremos modificar la mascara
		otra = (mode_t)strtol(cmd.argv[1], NULL, 8); //covertirmos el argumento de texto a numero en octal usando strtol 
		umask(otra); //Cambiamos la mascara
	}
}

void insertar(pid_t *pids, int n_pids, char *cmd){
	int i, j;
	for(i = 0; i < MAXJB; i++){ //insercion de una lista, recorremos la lista
		if(listaJobs[i].active == 0){  //verificamos si hay espacio vacio
			for(j = 0; j < n_pids && j < MAXARG; j++){
				listaJobs[i].pids[j] = pids[j];
			}
			listaJobs[i].n_pids = j;
			strncpy(listaJobs[i].cmd, cmd, MAXLINES - 1);
			listaJobs[i].cmd[MAXLINES - 1] = '\0';
			listaJobs[i].active = 1;
			listaJobs[i].orden = ++contadorJobs;
			return;
		}
	}
	printf("lista full\n");

}

void myjobs(){
	int i;
	for (i = 0; i < MAXJB; i++){ //recorremos la lista
		if(listaJobs[i].active == 1){ //imprimimos toda la lista
			printf("[%d] en background \t%s\n", i + 1, listaJobs[i].cmd);
		}
	}
	
}

void myfg(tcommand cmd){
	int pos, status, i;
	char *fin;
	long job;

	if(cmd.argc == 1){ //si solo pone fg se trae el ultimo trabajo activo
		pos = buscarUltimoJob();
		if(pos < 0){
			fprintf(stderr, "fg: no hay trabajos en background\n");
			return;
		}
	}else{
		job = strtol(cmd.argv[1], &fin, 10);
		if(*fin != '\0' || job < 1 || job > MAXJB){
			fprintf(stderr, "fg: identificador de trabajo no valido: %s\n", cmd.argv[1]);
			return;
		}
		pos = (int)job - 1; //necesitamos el id
	}

	if(pos < 0 || pos >= MAXJB || listaJobs[pos].active == 0){ //comprobamos si es valido
		fprintf(stderr, "fg: el trabajo no existe: %d\n", pos + 1);
		return;
	}
	printf("%s\n", listaJobs[pos].cmd); //imprimimos el nombre del command

	for(i = 0; i < listaJobs[pos].n_pids; i++){
		if(listaJobs[pos].pids[i] <= 0){
			continue;
		}
		while(waitpid(listaJobs[pos].pids[i], &status, 0) < 0){
			if(errno == EINTR){
				continue;
			}
			if(errno != ECHILD){
				perror("fg: waitpid");
			}
			break;
		}
		listaJobs[pos].pids[i] = 0;
	}

	listaJobs[pos].active = 0; //una vez que retorna lo devolvemos

}

void limpiarZombies(){
	pid_t pid;
	int status;
	int i, j, activos;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0){//no se bloquea, si hay hijos terminados devuelve un pid y recorremos la lista y lo borramos
		for(i = 0; i < MAXJB; i++){
			if(listaJobs[i].active){
				for(j = 0; j < listaJobs[i].n_pids; j++){
					if(listaJobs[i].pids[j] == pid){
						listaJobs[i].pids[j] = 0;
					}
				}

				activos = 0;
				for(j = 0; j < listaJobs[i].n_pids; j++){
					if(listaJobs[i].pids[j] > 0){
						activos = 1;
					}
				}
				if(!activos){
					listaJobs[i].active = 0;
				}
			}
		}
	}
}

int buscarUltimoJob(){
	int i, pos;
	unsigned long ultimo;

	pos = -1;
	ultimo = 0;
	for(i = 0; i < MAXJB; i++){
		if(listaJobs[i].active && listaJobs[i].orden > ultimo){
			ultimo = listaJobs[i].orden;
			pos = i;
		}
	}

	return pos;
}
