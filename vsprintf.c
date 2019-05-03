/*
 *  linux/kernel/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <stdarg.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>

unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
	unsigned long result = 0,value;

	if (!base) {
		base = 10;
		if (*cp == '0') {
			base = 8;
			cp++;
			if ((*cp == 'x') && isxdigit(cp[1])) {
				cp++;
				base = 16;
			}
		}
	}
	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
	    ? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}
	if (endp)
		*endp = (char *)cp;
	return result;
}

/* we use this so that we can do without the ctype library */
#define is_digit(c)	((c) >= '0' && (c) <= '9')    //判断字符c是否在0-9范围内

static int skip_atoi(const char **s)    //将数字字符转换成整型
{
	int i=0;

	while (is_digit(**s))
        //从高位开始，转换成整型后乘10，继续处理下一位
        //*s指的是s的指针，*（(*s)++)指的是先将高位转换完后指针后移一位，处理下一位字符
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define SMALL	64		/* use 'abcdef' instead of 'ABCDEF' */

//用n除以base，结果存入n，余数存入__res并返回
#define do_div(n,base) ({ \
int __res; \
__asm__("divl %4":"=a" (n),"=d" (__res):"0" (n),"1" (0),"r" (base)); \
__res; })

static char * number(char * str, int num, int base, int size, int precision
	,int type)
{
	char c,sign,tmp[36];
	const char *digits="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; //大写输出
	int i;

	if (type&SMALL) digits="0123456789abcdefghijklmnopqrstuvwxyz"; //小写输出
	if (type&LEFT) type &= ~ZEROPAD; //如果是左对齐就没有前导0
	if (base<2 || base>36) //进制必须在2-36之间
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ' ;//判断补0还是补空格
	if (type&SIGN && num<0) {   //判断是否有符号并且是否为负数
		sign='-';
		num = -num;  //相反数
	} else
		sign=(type&PLUS) ? '+' : ((type&SPACE) ? ' ' : 0);
	if (sign) size--; //如果有符号则宽度减1
	if (type&SPECIAL) //如果输入#
		if (base==16) size -= 2; //如果是16进制则输出0x 所以宽度减2
		else if (base==8) size--; //8进制输出0 宽度减1
	i=0;
	if (num==0)
		tmp[i++]='0'; //如果输出的是0则直接存到tmp中
	else while (num!=0)
		tmp[i++]=digits[do_div(num,base)];//如果非零则转换成base进制再存入
	if (i>precision) precision=i;//如果tmp的长度大于精度则按i的精度输出
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))//如果既不是左对齐又不是前导0则用空格补位
		while(size-->0)
			*str++ = ' ';
	if (sign)//输出符号
		*str++ = sign;
	if (type&SPECIAL)//如果是八进制则输出0，16进制输出0x或0X
		if (base==8)
			*str++ = '0';
		else if (base==16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	if (!(type&LEFT))//不是左对齐则用0或空格补位
		while(size-->0)
			*str++ = c;
	while(i<precision--)//如果精度大于实际长度则用0补位
		*str++ = '0';
	while(i-->0)
		*str++ = tmp[i];//输出数字部分
	while(size-->0)
		*str++ = ' ';//左对齐的话用空格补位
	return str;
}

int vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	int i;
	char * str;
	char *s;
	int *ip;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (str=buf ; *fmt ; ++fmt) {//输出中有无格式控制符
		if (*fmt != '%') {//如果不是%就直接输出无需处理
			*str++ = *fmt;
			continue;
		}
			
		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */ //跳过%
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;//左对齐
				case '+': flags |= PLUS; goto repeat;//正数输出+，负数输出-
				case ' ': flags |= SPACE; goto repeat;//无符号时插入空格
				case '#': flags |= SPECIAL; goto repeat;//处理#后面的情况如x等
				case '0': flags |= ZEROPAD; goto repeat;//前导0
				}
		
		/* get field width */
		field_width = -1;
		if (is_digit(*fmt))//将宽度控制的字符转换为整型
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {//如果是*则取args参数为宽度
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {//如果宽度为负数则默认宽度为1且默认左对齐
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {//如果有.则输出有精度要求
			++fmt;	//跳过.
			if (is_digit(*fmt))//将.后面的数字转换为整型并保存
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {//如果是*则取args参数为精度
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)//如果精度为负数则默认精度为0
				precision = 0;
		}

		/* get the conversion qualifier */
        //整型大小
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		switch (*fmt) {
		case 'c'://字符型
			if (!(flags & LEFT))//如果不是左对齐则空格补前面的位
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);//将int类型的存储的char转换为无符号字符型
			while (--field_width > 0)//按照宽度输出，空格补位
				*str++ = ' ';
			break;

		case 's'://字符串类型
			s = va_arg(args, char *);
			if (!s)//若传入的字符串为空则将s指向NULL
				s = "<NULL>";
			len = strlen(s);//得到字符串长度
			if (precision < 0)//若精度小于0则将精度就为长度
				precision = len;
			else if (len > precision)//若长度大于精度则将长度变为要求的精度
				len = precision;

			if (!(flags & LEFT))//如果不是左对齐且宽度大于实际长度，则空格补位
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)//输出字符串
				*str++ = *s++;
			while (len < field_width--)//按照宽度空格补位
				*str++ = ' ';
			break;

		case 'o'://八进制
			str = number(str, va_arg(args, unsigned long), 8,
				field_width, precision, flags);
			break;

		case 'p'://指针
			if (field_width == -1) {
				field_width = 8;
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			break;

		case 'x'://小写十六进制
			flags |= SMALL;
		case 'X'://大写十六进制
			str = number(str, va_arg(args, unsigned long), 16,
				field_width, precision, flags);
			break;

		case 'd'://有符号十进制
		case 'i':
			flags |= SIGN;
		case 'u'://无符号十进制
			str = number(str, va_arg(args, unsigned long), 10,
				field_width, precision, flags);
			break;

		case 'n'://向ip中存储到当前位置已经打印的字符数
			ip = va_arg(args, int *);
			*ip = (str - buf);
			break;

		default:
			if (*fmt != '%')//如果当前格式控制符不是%，则输出一个%，下一个if输出%后面不是上述情况的字符
				*str++ = '%';
			if (*fmt)//如果是%%则第一个if不输出%只有第二个%才输出一个%
				*str++ = *fmt;
			else
				--fmt;//如果%后面没有字符，则需要后退一位
			break;
		}
	}
	*str = '\0';//字符串结尾
	return str-buf;
}

int sprintf(char * buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	return i;
}

