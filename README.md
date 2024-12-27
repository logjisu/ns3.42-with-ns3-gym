# ns3.42-with-ns3-gym
 본 깃은 ns-3의 3.42 버전에 ns3-gym을 적용하는데 있어서 발생하는 오류를 해결하기 위해 작성되었습니다.<br>
ns-3 3.36+ 버전에 ns3-gym을 설치하려면 [ns3-gym 3.36+](https://github.com/tkn-tub/ns3-gym)을 참조해주세요.<br>
ns-3의 3.36이상의 버전은 ./waf 대신 ./run을 사용합니다. 또한, CMAKE를 사용하는 등 이전 버전의 ns-3에 비하여 내부적으로 많은 변화가 생겼습니다.<br>

## linear-mesh sim.cc
 공식적으로 ns3-gym을 설치하고 configure 및 build를 진행하면 linear-mesh sim.cc 파일에서 문제가 발생합니다.<br>
해당 오류는 ./contrib/opengym/examples/linear-mesh/sim.cc 파일에서 참조하는 파일의 SetTos 함수를 사용하려고 하지만 존재하지 않아서 발생합니다.<br>
SetTos 함수는 inet-socket-address라는 파일에서 사용되었던 함수입니다.<br>

### inet-socket-address
inet-socket-address 파일은 ns3가 설치된 경로의 아래에 존재합니다.<br>
```
./src/network/utils/inet-socket-address
```

해당 코드는 ns-3의 다른 버전에서 가져온 것입니다.<br>
1. inet-socket-address.h<br>
헤더 파일에는 SetTos 함수를 정의하고, m_tos라는 변수를 추가합니다.
```
public:
/**
* \param tos the new ToS.
*/
void SetTos (uint8_t tos);
private:
uint8_t m_tos;      //!< the Tos
```
2. inet-socket-address.cc<br>
실행 파일에는 헤더 파일애서 정의한 SetTos 함수를 구현합니다.
```
void
InetSocketAddress::SetTos (uint8_t tos)
{
  NS_LOG_FUNCTION (this << tos);
  m_tos = tos;
}
```
