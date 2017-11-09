#pragma once

typedef long long int64_t;
typedef unsigned long long uint64_t;

/// ��ȡ��ǰ���̵�cpuʹ���ʣ�����-1ʧ�� 
float get_cpu_usage(HANDLE hProcess);

/// ��ȡ��ǰ�����ڴ�������ڴ�ʹ����������-1ʧ�ܣ�0�ɹ� 
int get_memory_usage(HANDLE hProcess, uint64_t* mem, uint64_t* vmem, uint64_t* qp, uint64_t* qnp);

/// ��ȡ��ǰ�����ܹ�����д��IO�ֽ���������-1ʧ�ܣ�0�ɹ� 
int get_io_bytes(HANDLE hProcess, uint64_t* read_bytes, uint64_t* write_bytes);

unsigned int _stdcall information_collection(LPVOID pVoid);

//ԭ������GetProcessTimes()�������ϴε��õõ��Ľ����������õ�ĳ��ʱ����CPU��ʹ��ʱ��  
//C++ ��ȡ�ض����̹涨CPUʹ����  ԭ�ģ�http://blog.csdn.net/liuqx97bb/article/details/52058657  
class CPUusage {
private:
	HANDLE _hProcess;
	int _processor;    //cpu����    
	int64_t _last_time;         //��һ�ε�ʱ��    
	int64_t _last_system_time;


	// ʱ��ת��    
	uint64_t file_time_2_utc(const FILETIME* ftime);

	// ���CPU�ĺ���    
	int get_processor_number();

	//��ʼ��  
	void init()
	{
		_last_system_time = 0;
		_last_time = 0;
		_hProcess = 0;
	}

	//�رս��̾��  
	void clear()
	{
		if (_hProcess) {
			CloseHandle(_hProcess);
			_hProcess = 0;
		}
	}

public:
	CPUusage(DWORD ProcessID) {
		init();
		_processor = get_processor_number();
		setpid(ProcessID);
	}
	CPUusage() { init(); _processor = get_processor_number(); }
	~CPUusage() { clear(); }

	//����ֵΪ���̾�������ж�OpenProcess�Ƿ�ɹ�  
	HANDLE setpid(DWORD ProcessID) {
		clear();    //���֮ǰ���ӹ���һ�����̣����ȹر����ľ��  
		init();
		return _hProcess = OpenProcess(/*PROCESS_QUERY_LIMITED_INFORMATION | */PROCESS_QUERY_INFORMATION, false, ProcessID);
	}

	//-1 ��Ϊʧ�ܻ�������˳��� ����ɹ����״ε��û᷵��-2����;��setpid������PID���״ε���Ҳ�᷵��-2��  
	float get_cpu_usage();
};