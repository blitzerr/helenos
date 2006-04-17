/*
 * Copyright (C) 2001-2004 Jakub Jermar
 * Copyright (C) 2006 Josef Cejka
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <io/io.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#define __PRINTF_FLAG_PREFIX		0x00000001	/* show prefixes 0x or 0*/
#define __PRINTF_FLAG_SIGNED		0x00000002	/* signed / unsigned number */
#define __PRINTF_FLAG_ZEROPADDED	0x00000004	/* print leading zeroes */
#define __PRINTF_FLAG_LEFTALIGNED	0x00000010	/* align to left */
#define __PRINTF_FLAG_SHOWPLUS		0x00000020	/* always show + sign */
#define __PRINTF_FLAG_SPACESIGN		0x00000040	/* print space instead of plus */
#define __PRINTF_FLAG_BIGCHARS		0x00000080	/* show big characters */
#define __PRINTF_FLAG_NEGATIVE		0x00000100	/* number has - sign */

#define PRINT_NUMBER_BUFFER_SIZE	(64+5)		/* Buffer big enought for 64 bit number
							 * printed in base 2, sign, prefix and
							 * 0 to terminate string.. (last one is only for better testing 
							 * end of buffer by zero-filling subroutine)
							 */
typedef enum {
	PrintfQualifierByte = 0,
	PrintfQualifierShort,
	PrintfQualifierInt,
	PrintfQualifierLong,
	PrintfQualifierLongLong,
	PrintfQualifierSizeT,
	PrintfQualifierPointer
} qualifier_t;

static char digits_small[] = "0123456789abcdef"; 	/* Small hexadecimal characters */
static char digits_big[] = "0123456789ABCDEF"; 	/* Big hexadecimal characters */


/** Print one formatted character
 * @param c character to print
 * @param width 
 * @param flags
 * @return number of printed characters or EOF
 */
static int print_char(char c, int width, uint64_t flags)
{
	int counter = 0;
	
	if (!(flags & __PRINTF_FLAG_LEFTALIGNED)) {
		while (--width > 0) { 	/* one space is consumed by character itself hence predecrement */
			/* FIXME: painful slow */
			putchar(' ');	
			++counter;
		}
	}
	
 	if (putchar(c) == EOF) {
		return EOF;
	}

	while (--width > 0) { /* one space is consumed by character itself hence predecrement */
		putchar(' ');
		++counter;
	}
	
	return ++counter;
}

/** Print one string
 * @param s string
 * @param width 
 * @param precision
 * @param flags
 * @return number of printed characters or EOF
 */
						
static int print_string(char *s, int width, int precision, uint64_t flags)
{
	int counter = 0;
	size_t size;

	if (s == NULL) {
		return putstr("(NULL)");
	}
	
	size = strlen(s);

	/* print leading spaces */

	if (precision == 0) 
		precision = size;

	width -= precision;
	
	if (!(flags & __PRINTF_FLAG_LEFTALIGNED)) {
		while (width-- > 0) { 	
			putchar(' ');	
			counter++;
		}
	}

	while (precision > size) {
		precision--;
		putchar(' ');	
		++counter;
	}
	
 	if (putnchars(s, precision) == EOF) {
		return EOF;
	}

	counter += precision;

	while (width-- > 0) {
		putchar(' ');	
		++counter;
	}
	
	return ++counter;
}


/** Print number in given base
 *
 * Print significant digits of a number in given
 * base.
 *
 * @param num  Number to print.
 * @param width
 * @param precision
 * @param base Base to print the number in (should
 *             be in range 2 .. 16).
 * @param flags output modifiers
 * @return number of written characters or EOF
 *
 */
static int print_number(uint64_t num, int width, int precision, int base , uint64_t flags)
{
	char *digits = digits_small;
	char d[PRINT_NUMBER_BUFFER_SIZE];	/* this is good enough even for base == 2, prefix and sign */
	char *ptr = &d[PRINT_NUMBER_BUFFER_SIZE - 1];
	int size = 0;
	int written = 0;
	char sgn;
	
	if (flags & __PRINTF_FLAG_BIGCHARS) 
		digits = digits_big;	
	
	*ptr-- = 0; /* Put zero at end of string */

	if (num == 0) {
		*ptr-- = '0';
		size++;
	} else {
		do {
			*ptr-- = digits[num % base];
			size++;
		} while (num /= base);
	}

	/* Collect sum of all prefixes/signs/... to calculate padding and leading zeroes */
	if (flags & __PRINTF_FLAG_PREFIX) {
		switch(base) {
			case 2:	/* Binary formating is not standard, but usefull */
				size += 2;
				break;
			case 8:
				size++;
				break;
			case 16:
				size += 2;
				break;
		}
	}

	sgn = 0;
	if (flags & __PRINTF_FLAG_SIGNED) {
		if (flags & __PRINTF_FLAG_NEGATIVE) {
			sgn = '-';
			size++;
		} else if (flags & __PRINTF_FLAG_SHOWPLUS) {
				sgn = '+';
				size++;
			} else if (flags & __PRINTF_FLAG_SPACESIGN) {
					sgn = ' ';
					size++;
				}
	}

	if (flags & __PRINTF_FLAG_LEFTALIGNED) {
		flags &= ~__PRINTF_FLAG_ZEROPADDED;
	}

	/* if number is leftaligned or precision is specified then zeropadding is ignored */
	if (flags & __PRINTF_FLAG_ZEROPADDED) {
		if ((precision == 0) && (width > size)) {
			precision = width - size;
		}
	}

	/* print leading spaces */
	if (size > precision) /* We must print whole number not only a part */
		precision = size;

	width -= precision;
	
	if (!(flags & __PRINTF_FLAG_LEFTALIGNED)) {
		while (width-- > 0) { 	
			putchar(' ');	
			written++;
		}
	}
	
	/* print sign */
	if (sgn) {
		putchar(sgn);
		written++;
	}
	
	/* print prefix */
	
	if (flags & __PRINTF_FLAG_PREFIX) {
		switch(base) {
			case 2:	/* Binary formating is not standard, but usefull */
				putchar('0');
				if (flags & __PRINTF_FLAG_BIGCHARS) {
					putchar('B');
				} else {
					putchar('b');
				}
				written += 2;
				break;
			case 8:
				putchar('o');
				written++;
				break;
			case 16:
				putchar('0');
				if (flags & __PRINTF_FLAG_BIGCHARS) {
					putchar('X');
				} else {
					putchar('x');
				}
				written += 2;
				break;
		}
	}

	/* print leading zeroes */
	precision -= size;
	while (precision-- > 0) { 	
		putchar('0');	
		written++;
	}

	
	/* print number itself */

	written += putstr(++ptr);
	
	/* print ending spaces */
	
	while (width-- > 0) { 	
		putchar(' ');	
		written++;
	}

	return written;
}


/** General formatted text print
 *
 * Print string formatted according the fmt parameter
 * and variant arguments. Each formatting directive
 * must have the following form:
 * % [ flags ] [ width ] [ .precision ] [ type ] conversion
 *
 * FLAGS:
 * #	Force to print prefix. For conversion %o is prefix 0, for %x and %X are prefixes 0x and 0X and for conversion %b is prefix 0b.
 * -	Align to left.
 * +	Print positive sign just as negative.
 *   (space)	If printed number is positive and '+' flag is not set, print space in place of sign.
 * 0	Print 0 as padding instead of spaces. Zeroes are placed between sign and the rest of number. This flag is ignored if '-' flag is specified.
 * 
 * WIDTH:
 * Specify minimal width of printed argument. If it is bigger, width is ignored.
 * If width is specified with a '*' character instead of number, width is taken from parameter list. 
 * Int parameter expected before parameter for processed conversion specification.
 * If this value is negative it is taken its absolute value and the '-' flag is set.
 *
 * PRECISION:
 * Value precision. For numbers it specifies minimum valid numbers.
 * Smaller numbers are printed with leading zeroes. Bigger numbers are not affected.
 * Strings with more than precision characters are cutted of.
 * Just as width could be '*' used instead a number. 
 * A int value is then expected in parameters. When both width and precision are specified using '*',
 * first parameter is used for width and second one for precision.
 * 
 * TYPE:
 * hh	signed or unsigned char
 * h	signed or usigned short
 * 	signed or usigned int (default value)
 * l	signed or usigned long int
 * ll	signed or usigned long long int
 * z	size_t type
 * 
 * 
 * CONVERSIONS:
 * 
 * %	Print percentage character.
 *
 * c	Print single character.
 *
 * s	Print zero terminated string. If a NULL value is passed as value, "(NULL)" is printed instead.
 * 
 * P, p	Print value of a pointer. Void * value is expected and it is printed in hexadecimal notation with prefix
 * ( as with %#X or %#x for 32bit or %#X / %#x for 64bit long pointers)
 *
 * b	Print value as unsigned binary number.  Prefix is not printed by default. (Nonstandard extension.)
 * 
 * o	Print value as unsigned octal number. Prefix is not printed by default. 
 *
 * d,i	Print signed decimal number. There is no difference between d and i conversion.
 *
 * u	Print unsigned decimal number.
 *
 * X, x	Print hexadecimal number with upper- or lower-case. Prefix is not printed by default. 
 * 
 * All other characters from fmt except the formatting directives
 * are printed in verbatim.
 *
 * @param fmt Formatting NULL terminated string.
 * @return count of printed characters or negative value on fail.
 */
int printf(const char *fmt, ...)
{
	int i = 0, j = 0; /* i is index of currently processed char from fmt, j is index to the first not printed nonformating character */
	int end;
	int counter; /* counter of printed characters */
	int retval; /* used to store return values from called functions */
	va_list ap;
	char c;
	qualifier_t qualifier;	/* type of argument */
	int base;	/* base in which will be parameter (numbers only) printed */
	uint64_t number; /* argument value */
	size_t	size; /* byte size of integer parameter */
	int width, precision;
	uint64_t flags;
	
	counter = 0;
	va_start(ap, fmt);
	
	while ((c = fmt[i])) {
		/* control character */
		if (c == '%' ) { 
			/* print common characters if any processed */	
			if (i > j) {
				if ((retval = putnchars(&fmt[j], (size_t)(i - j))) == EOF) { /* error */
					return -counter;
				}
				counter += retval;
			}
		
			j = i;
			/* parse modifiers */
			flags = 0;
			end = 0;
			
			do {
				++i;
				switch (c = fmt[i]) {
					case '#': flags |= __PRINTF_FLAG_PREFIX; break;
					case '-': flags |= __PRINTF_FLAG_LEFTALIGNED; break;
					case '+': flags |= __PRINTF_FLAG_SHOWPLUS; break;
					case ' ': flags |= __PRINTF_FLAG_SPACESIGN; break;
					case '0': flags |= __PRINTF_FLAG_ZEROPADDED; break;
					default: end = 1;
				};	
				
			} while (end == 0);	
			
			/* width & '*' operator */
			width = 0;
			if (isdigit(fmt[i])) {
				while (isdigit(fmt[i])) {
					width *= 10;
					width += fmt[i++] - '0';
				}
			} else if (fmt[i] == '*') {
				/* get width value from argument list*/
				i++;
				width = (int)va_arg(ap, int);
				if (width < 0) {
					/* negative width means to set '-' flag */
					width *= -1;
					flags |= __PRINTF_FLAG_LEFTALIGNED;
				}
			}
			
			/* precision and '*' operator */	
			precision = 0;
			if (fmt[i] == '.') {
				++i;
				if (isdigit(fmt[i])) {
					while (isdigit(fmt[i])) {
						precision *= 10;
						precision += fmt[i++] - '0';
					}
				} else if (fmt[i] == '*') {
					/* get precision value from argument list*/
					i++;
					precision = (int)va_arg(ap, int);
					if (precision < 0) {
						/* negative precision means to ignore it */
						precision = 0;
					}
				}
			}

			switch (fmt[i++]) {
				/** TODO: unimplemented qualifiers:
				 * t ptrdiff_t - ISO C 99
				 */
				case 'h':	/* char or short */
					qualifier = PrintfQualifierShort;
					if (fmt[i] == 'h') {
						i++;
						qualifier = PrintfQualifierByte;
					}
					break;
				case 'l':	/* long or long long*/
					qualifier = PrintfQualifierLong;
					if (fmt[i] == 'l') {
						i++;
						qualifier = PrintfQualifierLongLong;
					}
					break;
				case 'z':	/* size_t */
					qualifier = PrintfQualifierSizeT;
					break;
				default:
					qualifier = PrintfQualifierInt; /* default type */
					--i;
			}	
			
			base = 10;

			switch (c = fmt[i]) {

				/*
				* String and character conversions.
				*/
				case 's':
					if ((retval = print_string(va_arg(ap, char*), width, precision, flags)) == EOF) {
						return -counter;
					};
					
					counter += retval;
					j = i + 1; 
					goto next_char;
				case 'c':
					c = va_arg(ap, unsigned int);
					if ((retval = print_char(c, width, flags )) == EOF) {
						return -counter;
					};
					
					counter += retval;
					j = i + 1;
					goto next_char;

				/* 
				 * Integer values
				*/
				case 'P': /* pointer */
				       	flags |= __PRINTF_FLAG_BIGCHARS;
				case 'p':
					flags |= __PRINTF_FLAG_PREFIX;
					base = 16;
					qualifier = PrintfQualifierPointer;
					break;	
				case 'b': 
					base = 2;
					break;
				case 'o':
					base = 8;
					break;
				case 'd':
				case 'i':
					flags |= __PRINTF_FLAG_SIGNED;  
				case 'u':
					break;
				case 'X':
					flags |= __PRINTF_FLAG_BIGCHARS;
				case 'x':
					base = 16;
					break;
				/* percentile itself */
				case '%': 
					j = i;
					goto next_char;
				/*
				* Bad formatting.
				*/
				default:
					/* Unknown format
					 *  now, the j is index of '%' so we will
					 * print whole bad format sequence
					 */
					goto next_char;		
			}
		
		
		/* Print integers */
			/* print number */
			switch (qualifier) {
				case PrintfQualifierByte:
					size = sizeof(unsigned char);
					number = (uint64_t)va_arg(ap, unsigned int);
					break;
				case PrintfQualifierShort:
					size = sizeof(unsigned short);
					number = (uint64_t)va_arg(ap, unsigned int);
					break;
				case PrintfQualifierInt:
					size = sizeof(unsigned int);
					number = (uint64_t)va_arg(ap, unsigned int);
					break;
				case PrintfQualifierLong:
					size = sizeof(unsigned long);
					number = (uint64_t)va_arg(ap, unsigned long);
					break;
				case PrintfQualifierLongLong:
					size = sizeof(unsigned long long);
					number = (uint64_t)va_arg(ap, unsigned long long);
					break;
				case PrintfQualifierPointer:
					size = sizeof(void *);
					number = (uint64_t)(unsigned long)va_arg(ap, void *);
					break;
				case PrintfQualifierSizeT:
					size = sizeof(size_t);
					number = (uint64_t)va_arg(ap, size_t);
					break;
				default: /* Unknown qualifier */
					return -counter;
					
			}
			
			if (flags & __PRINTF_FLAG_SIGNED) {
				if (number & (0x1 << (size*8 - 1))) {
					flags |= __PRINTF_FLAG_NEGATIVE;
				
					if (size == sizeof(uint64_t)) {
						number = -((int64_t)number);
					} else {
						number = ~number;
						number &= (~((0xFFFFFFFFFFFFFFFFll) <<  (size * 8)));
						number++;
					}
				}
			}

			if ((retval = print_number(number, width, precision, base, flags)) == EOF ) {
				return -counter;
			};

			counter += retval;
			j = i + 1;
		}	
next_char:
			
		++i;
	}
	
	if (i > j) {
		if ((retval = putnchars(&fmt[j], (size_t)(i - j))) == EOF) { /* error */
			return -counter;
		}
		counter += retval;
	}
	
	va_end(ap);
	return counter;
}

