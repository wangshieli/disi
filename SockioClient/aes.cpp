#include "aes.h"
#include "base64.h"
#include <openssl\aes.h>
#include "../ShareApi/ShareApi.h"

char password_[] = {
	'_',  '+', '-', '*', '^', '%', '$', '#', '@', 
	'!', '?', '(', ')', '[', ']', '{', '}', '='
};

BOOL isPoint(int _c)
{
	for (int i = 0; i < sizeof(password_); i++)
	{
		if (_c == (int)password_[i])
			return TRUE;
	}
	return FALSE;
}

void GetPassWord(const char* pInfo, char **password)
{
	AES_KEY aes;
	unsigned char key[] = "97fd0P5ne0nRBs0v4rRgoY9JKSik4I9i";
	unsigned char iv_d[] = "F$~((kb~AjO*xgn~";
	unsigned char* d_string;
	unsigned int i = 0;

	char buf[512] = {0};
	int len = Base64Decode(buf, (char*)pInfo, 0);

	AES_set_decrypt_key(key, 256, &aes);
	d_string = (unsigned char*)calloc(len+8, sizeof(unsigned char));
	AES_cbc_encrypt((unsigned char*)buf, d_string, len, &aes, iv_d, AES_DECRYPT);
	//printf("input = %s\n", pInfo); 
	for (int i = 0; i < len; i++)
	{
		if (isalpha(d_string[i]) || isdigit(d_string[i]) || isPoint(d_string[i]))
			continue;
		else
		{
			d_string[i] = '\0';
			break;
		}
	}
//	printf("decrypted string = %s\n", (char*)d_string);
	*password = (char*)d_string;
}