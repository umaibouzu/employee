#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>

#include "common.h"
#include "parse.h"

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {

	if (dbhdr == NULL) return; 

	int i = 0;

	for (; i < dbhdr->count; i++) {
		printf("Employee %d\n", i);
		printf("\tName: %s\n", employees[i].name);
		printf("\tAddress: %s\n", employees[i].address);
		printf("\tHours: %d\n", employees[i].hours);
	}
}

int update_employee(struct dbheader_t *dbhdr, struct employee_t *employees, char *updatestring) {

	if (dbhdr == NULL) return STATUS_ERROR;

	char *name =  strtok(updatestring, ",");
	if (name == NULL) return STATUS_ERROR;

	char *hours = strtok(NULL, ",");
	if (hours == NULL) return STATUS_ERROR;

	int index = 0;

	for (; index < dbhdr->count; index++) {
		if (strcmp(employees[index].name, name) == 0) {
			break;
		}
	}
	
	employees[index].hours = atoi(hours);

	return STATUS_SUCCESS;
}


int remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *removestring) {

	if (dbhdr == NULL) return STATUS_ERROR;
	if (employees == NULL) return STATUS_ERROR;
	if (*employees == NULL) return STATUS_ERROR;
	if (removestring == NULL) return STATUS_ERROR;

	if (dbhdr->count == 1) {
		free(*employees);
		*employees = NULL;
		dbhdr->count = 0;
		return STATUS_SUCCESS;
	}

	struct employee_t *e = *employees;
	
	int index = 0;
	bool found = false;

	for (; index < dbhdr->count; index++) {
		if (strcmp(e[index].name, removestring) == 0) {
			found = true;
			break;
		}
	}

	int i = index;

	if (found) {
		for (; i < dbhdr->count - 1; i++) {
			e[i] = e[i+1];
		}
	}else {
		printf("No name in database matching: %s\n", removestring);
		return STATUS_ERROR;
	}

	e = realloc(e, sizeof(struct employee_t) * (dbhdr->count-1));
	if (e == NULL) {
		return STATUS_ERROR;
	}

	dbhdr->count--;

	*employees = e;

	return STATUS_SUCCESS;
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring) {

	if (dbhdr == NULL) return STATUS_ERROR;
	if (employees == NULL) return STATUS_ERROR;
	if (*employees == NULL) return STATUS_ERROR;
	if (addstring == NULL) return STATUS_ERROR;


	char *name =  strtok(addstring, ",");
	if (name == NULL) return STATUS_ERROR;

	char *addr = strtok(NULL, ",");
	if (addr == NULL) return STATUS_ERROR;

	char *hours = strtok(NULL, ",");
	if (hours == NULL) return STATUS_ERROR;

	struct employee_t *e = *employees;
	e = realloc(e, sizeof(struct employee_t) * (dbhdr->count+1));
	if (e == NULL) {
		return STATUS_ERROR;
	}

	dbhdr->count++;

	strncpy(e[dbhdr->count-1].name, name, sizeof(e[dbhdr->count-1].name)-1);
	strncpy(e[dbhdr->count-1].address, addr, sizeof(e[dbhdr->count-1].address)-1);
	e[dbhdr->count-1].hours = atoi(hours);

	*employees = e;

	return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
	if (fd < 0) {
		printf("Got a bad FD from the user\n");
		return STATUS_ERROR;
	}

	int count = dbhdr->count;

	struct employee_t *employees = calloc(count, sizeof(struct employee_t));
	if (employees == NULL) {
		printf("Malloc failed\n");
		return STATUS_ERROR;
	}

	read(fd, employees, count*sizeof(struct employee_t));

	int i = 0;
	for (; i < count; i++) {
		employees[i].hours = ntohl(employees[i].hours);
	}

	*employeesOut = employees;
	return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees) {
	if (fd < 0) {
		printf("Got a bad FD from the user\n");
		return STATUS_ERROR;
	}

	int realcount = dbhdr->count;
	off_t newsize = sizeof(struct dbheader_t) + (sizeof(struct employee_t) * realcount);

	dbhdr->magic = htonl(dbhdr->magic);
	//dbhdr->filesize = htonl(sizeof(struct dbheader_t) + sizeof(struct employee_t) * realcount);
	dbhdr->filesize = htonl(newsize);
	dbhdr->count = htons(dbhdr->count);
	dbhdr->version = htons(dbhdr->version);

	lseek(fd, 0, SEEK_SET);

	write(fd, dbhdr, sizeof(struct dbheader_t));

	int i = 0;

	for (; i < realcount; i++) {
		employees[i].hours = htonl(employees[i].hours);
		write(fd, &employees[i], sizeof(struct employee_t));
	}

	if (ftruncate(fd, newsize) == -1) {
		perror("truncate");
		return STATUS_ERROR;
	}

	return STATUS_SUCCESS;
}

int create_db_header(struct dbheader_t **headerOut) {
	struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
	if (header == NULL) {
		printf("Malloc failed to create db header\n");
		return STATUS_ERROR;
	}


	header->version = 0x1;
	header->count = 0;
	header->magic = HEADER_MAGIC;
	header->filesize = sizeof(struct dbheader_t);

	*headerOut = header;

	return STATUS_SUCCESS;
}



int validate_db_header(int fd, struct dbheader_t **headerOut) {
	if (fd < 0) {
		printf("Got a bad FD from the user\n");
		return STATUS_ERROR;
	}

	struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
	if (header == NULL) {
		printf("Malloc failed to create db header\n");
		return STATUS_ERROR;
	}

	if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
		perror("read");
		free(header);
		return STATUS_ERROR;
	}

	header->version = ntohs(header->version);
	header->count = ntohs(header->count);
	header->magic = ntohl(header->magic);
	header->filesize = ntohl(header->filesize);

	if (header->version != 1) {
		printf("Improper header version\n");
		free(header);
		return STATUS_ERROR;
	}
	if (header->magic != HEADER_MAGIC) {
		printf("Improper header magic\n");
		free(header);
		return STATUS_ERROR;
	}

	struct stat dbstat = {0};
	fstat(fd, &dbstat);
	if (header->filesize != dbstat.st_size) {
		printf("Corrupted database\n");
		free(header);
		return STATUS_ERROR;
	}

	*headerOut = header;
	return STATUS_SUCCESS;
}

