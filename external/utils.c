int scmp( unsigned char *s1, unsigned char *s2 )
{
    while( *s1 != '\0' && *s1 == *s2 )
    {
        s1++;
        s2++;
    }
    return( *s1-*s2 );
}

void
inssort(unsigned char** a, int n, int d)
{
	unsigned char** pi;
	unsigned char** pj;
	unsigned char* s;
	unsigned char* t;

	for (pi = a + 1; --n > 0; pi++) {
		unsigned char* tmp = *pi;

		for (pj = pi; pj > a; pj--) {
			for (s=*(pj-1)+d, t=tmp+d; *s==*t && *s!=0; ++s, ++t)
				;
			if (*s <= *t)
				break;
			*pj = *(pj-1);
		}
		*pj = tmp;
	}
}

