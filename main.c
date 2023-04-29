#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { nelements_max = 100 };
int main(int argc, char **argv) {
  char element[2048];
  char line[2048];
  char path[2048 + 6];
  float x[3];
  double lo[3];
  double hi[3];
  int64_t i;
  int64_t id;
  int64_t j;
  int64_t natoms;
  int64_t natoms_min;
  int64_t natoms_max;
  int64_t nelements;
  int64_t timestep;
  int64_t type;
  int64_t time;
  int Verbose;
  struct {
    char *element;
    FILE *raw;
    FILE *xdmf;
    long natoms;
    long pos;
    long seek;
  } files[nelements_max];
  Verbose = 0;
  natoms_max = 0;
  natoms_min = 0;
  while (*++argv != NULL && argv[0][0] == '-')
    switch (argv[0][1]) {
    case 'h':
      fprintf(stderr, "usage: lmp2raw [-v] < lammps.dump\n");
      exit(1);
    case 'v':
      Verbose = 1;
      break;
    default:
      fprintf(stderr, "lmp2raw: unknown option '%s'\n", *argv);
      exit(1);
    }
  nelements = 0;
  for (time = 0;; time++) {
    do {
      if (fgets(line, sizeof line, stdin) == NULL)
        goto end;
    } while (strncmp("ITEM: TIMESTEP", line, 14) != 0);
    if (scanf("%" PRId64, &timestep) != 1) {
      fprintf(stderr, "lmp2raw: fail to read the number of atoms\n");
      exit(2);
    }
    do {
      if (fgets(line, sizeof line, stdin) == NULL) {
        fprintf(stderr, "lmp2raw: no ITEM: ATOMS section\n");
        exit(2);
      }
    } while (strncmp("ITEM: NUMBER OF ATOMS", line, 21) != 0);
    if (scanf("%" PRId64, &natoms) != 1) {
      fprintf(stderr, "lmp2raw: fail to read the number of atoms\n");
      exit(2);
    }
    if (natoms < natoms_min || natoms_max == 0)
      natoms_min = natoms;
    if (natoms > natoms_max)
      natoms_max = natoms;
    do {
      if (fgets(line, sizeof line, stdin) == NULL) {
        fprintf(stderr, "lmp2raw: no ITEM: BOX BOUNDS section\n");
        exit(2);
      }
    } while (strncmp("ITEM: BOX BOUNDS", line, 16) != 0);
    if ((scanf("%lf %lf", &lo[0], &hi[0]) != 2) ||
        (scanf("%lf %lf", &lo[1], &hi[1]) != 2) ||
        (scanf("%lf %lf", &lo[2], &hi[2]) != 2)) {
      fprintf(stderr, "lmp2raw: fail to read box bounds\n");
      exit(2);
    }
    do {
      if (fgets(line, sizeof line, stdin) == NULL) {
        fprintf(stderr, "lmp2raw: no ITEM: ATOMS section\n");
        exit(2);
      }
    } while (strncmp("ITEM: ATOMS", line, 11) != 0);
    for (i = 0; i < natoms; i++) {
      if (scanf("%" PRId64 " %" PRId64 " %s %f %f %f", &id, &type, element,
                &x[0], &x[1], &x[2]) != 6) {
        fprintf(stderr, "lmp2raw: fail to parse atom line\n");
        exit(2);
      }

      for (j = 0; j < nelements; j++) {
        if (strcmp(files[j].element, element) == 0)
          break;
      }
      if (j == nelements_max) {
        fprintf(stderr, "lmp2raw: too many different elements\n");
        exit(2);
      }
      if (j == nelements) {
        if ((files[j].element = malloc(strlen(element) + 1)) == NULL) {
          fprintf(stderr, "lmp2raw: malloc failed\n");
          exit(2);
        }
        strcpy(files[j].element, element);
        snprintf(path, sizeof path, "%s.raw", element);
        if ((files[j].raw = fopen(path, "w")) == NULL) {
          fprintf(stderr, "lmp2raw: failed to open '%s'\n", path);
          exit(2);
        }
        if (Verbose)
          fprintf(stderr, "lmp2raw: %s\n", path);
        snprintf(path, sizeof path, "%s.xdmf2", element);
        if ((files[j].xdmf = fopen(path, "w")) == NULL) {
          fprintf(stderr, "lmp2raw: failed to open '%s'\n", path);
          exit(2);
        }
        if (Verbose)
          fprintf(stderr, "lmp2raw: %s\n", path);

        fprintf(files[j].xdmf, "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<Xdmf\n\
    Version=\"2.0\">\n\
  <Domain>\n\
    <Grid\n\
	CollectionType=\"Temporal\"\n\
	GridType=\"Collection\">\n\
");
        files[j].natoms = 0;
        files[j].seek = 0;
        files[j].pos = 0;
        nelements++;
      }
      if (fwrite(x, sizeof x, 1, files[j].raw) != 1) {
        fprintf(stderr, "lmp2raw: fail to write '%s' binary file\n",
                files[j].element);
        exit(2);
      }
      files[j].natoms++;
      files[j].pos += sizeof x;
    }
    for (j = 0; j < nelements; j++) {
      fprintf(files[j].xdmf, "\
    <Grid>\n\
      <Time Value=\"%" PRId64 "\"/>\n\
      <Geometry\n\
	  GeometryType=\"XYZ\">\n\
	<DataItem\n\
	    Dimensions=\"%" PRId64 " 3\"\n\
	    Format=\"Binary\"\n\
	    Seek=\"%" PRId64 "\">\n\
	  %s.raw\n\
	</DataItem>\n\
      </Geometry>\n\
      <Topology\n\
	  TopologyType=\"Polyvertex\">\n\
      </Topology>\n\
    </Grid>\n\
",
              timestep, files[j].natoms, files[j].seek, files[j].element);
      files[j].natoms = 0;
      files[j].seek = files[j].pos;
    }
  }
end:
  if (Verbose) {
    fprintf(stderr, "lmp2raw: ntimestep: %" PRId64 "\n", time);
    fprintf(stderr, "lmp2raw: natoms_min: %" PRId64 "\n", natoms_min);
    fprintf(stderr, "lmp2raw: natoms_max: %" PRId64 "\n", natoms_max);
    fprintf(stderr, "lmp2raw: xlo, xhi: %.16e %.16e\n", lo[0], hi[0]);
    fprintf(stderr, "lmp2raw: ylo, yhi: %.16e %.16e\n", lo[1], hi[1]);
    fprintf(stderr, "lmp2raw: zlo, zhi: %.16e %.16e\n", lo[2], hi[2]);
  }
  for (j = 0; j < nelements; j++) {
    free(files[j].element);
    if (fclose(files[j].raw) != 0)
      fprintf(stderr, "lmp2raw: fail to close a raw file\n");
    fprintf(files[j].xdmf, "\
    </Grid>\n\
  </Domain>\n\
</Xdmf>\n\
");
    if (fclose(files[j].xdmf) != 0)
      fprintf(stderr, "lmp2raw: fail to close an XDMF file\n");
  }
}
