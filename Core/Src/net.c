#include "net.h"
//--------------------------------------------------
extern UART_HandleTypeDef huart1;
extern uint8_t macaddr[6];
//--------------------------------------------------
uint8_t net_buf[ENC28J60_MAXFRAME];
uint8_t ipaddr[4]=IP_ADDR;
char str1[60]={0};
extern char str[20];
USART_prop_ptr usartprop;
//--------------------------------------------------
void eth_send(enc28j60_frame_ptr *frame, uint16_t len)
{
  memcpy(frame->addr_dest,frame->addr_src,6);
  memcpy(frame->addr_src,macaddr,6);
  enc28j60_packetSend((void*)frame,len + sizeof(enc28j60_frame_ptr));
}
//--------------------------------------------------
uint16_t checksum(uint8_t *ptr, uint16_t len)
{
	uint32_t sum=0;
	while(len>1)
	{
		sum += (uint16_t) (((uint32_t)*ptr<<8)|*(ptr+1));
		ptr+=2;
		len-=2;
	}
	if(len) sum+=((uint32_t)*ptr)<<8;
	while (sum>>16) sum=(uint16_t)sum+(sum>>16);
	return ~be16toword((uint16_t)sum);
}
//--------------------------------------------------
uint8_t ip_send(enc28j60_frame_ptr *frame, uint16_t len)
{
	uint8_t res=0;
	ip_pkt_ptr *ip_pkt = (void*)frame->data;
	//Заполним заголовок пакета IP
	ip_pkt->len=be16toword(len);
	ip_pkt->fl_frg_of=0;
	ip_pkt->ttl=128;
	ip_pkt->cs = 0;
	memcpy(ip_pkt->ipaddr_dst,ip_pkt->ipaddr_src,4);
	memcpy(ip_pkt->ipaddr_src,ipaddr,4);
	ip_pkt->cs = checksum((void*)ip_pkt,sizeof(ip_pkt_ptr));
	//отправим фрейм
	eth_send(frame,len);
	return res;
}
//--------------------------------------------------
uint8_t icmp_read(enc28j60_frame_ptr *frame, uint16_t len)
{
	uint8_t res=0;
	ip_pkt_ptr *ip_pkt = (void*)frame->data;
	icmp_pkt_ptr *icmp_pkt = (void*)ip_pkt->data;
	//Отфильтруем пакет по длине и типу сообщения - эхо-запрос
	if ((len>=sizeof(icmp_pkt_ptr))&&(icmp_pkt->msg_tp==ICMP_REQ))
	{
		sprintf(str1,"icmp request\r\n");
		HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
		icmp_pkt->msg_tp=ICMP_REPLY;
		icmp_pkt->cs=0;
		icmp_pkt->cs=checksum((void*)icmp_pkt,len);
		ip_send(frame,len+sizeof(ip_pkt_ptr));
	}
  return res;
}
//--------------------------------------------------
uint8_t ip_read(enc28j60_frame_ptr *frame, uint16_t len)
{
	uint8_t res=0;
  ip_pkt_ptr *ip_pkt = (void*)(frame->data);
	if((ip_pkt->verlen==0x45)&&(!memcmp(ip_pkt->ipaddr_dst,ipaddr,4)))
  {
		//длина данных
		len = be16toword(ip_pkt->len) - sizeof(ip_pkt_ptr);
		if (ip_pkt->prt==IP_ICMP)
		{
			icmp_read(frame,len);
		}
		else if (ip_pkt->prt==IP_TCP)
		{
		
		}
		else if (ip_pkt->prt==IP_UDP)
		{
		
		}
  }
  return res;
}
//--------------------------------------------------
void eth_read(enc28j60_frame_ptr *frame, uint16_t len)
{
	if (len>=sizeof(enc28j60_frame_ptr))
  {
    if(frame->type==ETH_ARP)
		{
			sprintf(str1,"%02X:%02X:%02X:%02X:%02X:%02X-%02X:%02X:%02X:%02X:%02X:%02X; %d; arp\r\n",
			frame->addr_src[0],frame->addr_src[1],frame->addr_src[2],frame->addr_src[3],frame->addr_src[4],frame->addr_src[5],
			frame->addr_dest[0],frame->addr_dest[1],frame->addr_dest[2],frame->addr_dest[3],frame->addr_dest[4],frame->addr_dest[5],
			len);
			HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
			arp_read(frame, len - sizeof(enc28j60_frame_ptr));
			if(arp_read(frame,len-sizeof(enc28j60_frame_ptr)))
			{
				arp_send(frame);
			}
		}
		else if(frame->type==ETH_IP)
		{
			sprintf(str1,"%02X:%02X:%02X:%02X:%02X:%02X-%02X:%02X:%02X:%02X:%02X:%02X; %d; ip\r\n",
			frame->addr_src[0],frame->addr_src[1],frame->addr_src[2],frame->addr_src[3],frame->addr_src[4],frame->addr_src[5],
			frame->addr_dest[0],frame->addr_dest[1],frame->addr_dest[2],frame->addr_dest[3],frame->addr_dest[4],frame->addr_dest[5],
			len);
			HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
			ip_read(frame,len-sizeof(ip_pkt_ptr));
		}
  }
}
//--------------------------------------------------
void net_pool(void)
{
	uint16_t len;
  enc28j60_frame_ptr *frame=(void*)net_buf;
	while ((len=enc28j60_packetReceive(net_buf,sizeof(net_buf)))>0)
  {
    eth_read(frame,len);
  }
}
//--------------------------------------------------
void net_ini()
{
	usartprop.usart_buf[0]=0;
  usartprop.usart_cnt=0;
  usartprop.is_ip=0;
  HAL_UART_Transmit(&huart1,(uint8_t*)"123456rn",8,0x1000);
	enc28j60_ini();
}
//--------------------------------------------------
void UART1_RxCpltCallback(void)
{
	uint8_t b;
  b = str[0];
	//если вдруг случайно превысим длину буфера
  if (usartprop.usart_cnt>20)
  {
    usartprop.usart_cnt=0;
  }
  else if (b == 'a')
  {
    usartprop.is_ip=1;//статус отрпавки ARP-запроса
  }
  else
  {
    usartprop.usart_buf[usartprop.usart_cnt] = b;
    usartprop.usart_cnt++;
  }
  HAL_UART_Receive_IT(&huart1,(uint8_t*)str,1);
}
//-----------------------------------------------
