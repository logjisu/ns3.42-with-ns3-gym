import gym                  #
import numpy as np          #
import ns3gym               # ns3-gym 라이브러리
import random               #
import time                 #
from ns3gym import ns3env   #

# Gym 환경 설정
port = 5555     # 포트
seed = 8      # 시드
env = ns3env.Ns3Env(port=port, startSim=True, simSeed=seed) # 환경 설정
print(f"NS-3 포트({port})에 성공적으로 연결되었습니다.")       # 성공적 연결
state_space = env.observation_space.shape[0]                # 
action_space = env.action_space.shape[0]                    # 

# Q-learning 파라미터
alpha = 0.6             # 학습률
gamma = 0.9             # 할인율
epsilon = 0.7           # 탐험률
epsilon_decay = 0.99    # 탐험률 감소
epsilon_min = 0.01      # 최소 탐험률
episodes = 100          # 에피소드 수

# Q-테이블 초기화
q_table = np.zeros((3, 10))     # (상태 공간 크기: 3, 행동 공간 크기: 2)


def discretize_state(state):
    try:
        if not isinstance(state, (list, np.ndarray)) or len(state) == 0:
            print(f"잘못된 상태 encountered: {state}")
            return 0  # 기본값

        q_value = int(state[0] - 1)  # QValue: 1~3을 0~2로 변환
        q_value = max(0, min(q_value, q_table.shape[0] - 1))  # 범위 제한
        return q_value
    except Exception as e:
        print(f"Error in discretize_state: {e}")
        return 0


def choose_action(state, epsilon):
    q_value = discretize_state(state)

    if np.random.rand() < epsilon:
        action_index = random.randint(0, q_table.shape[1] - 1)  # 랜덤 행동
    else:
        action_index = np.argmax(q_table[q_value])  # Q-테이블에서 최적 행동 선택

    # 행동 값 계산
    action_value = 2.0 + action_index * 0.3
    return [action_value, action_value]


def update_q_table(state, action, reward, next_state):
    q_value = discretize_state(state)
    next_q_value = discretize_state(next_state)

    try:
        action_index = int((action[0] - 2.0) / 0.3)  # 행동 인덱스 계산
        action_index = max(0, min(action_index, q_table.shape[1] - 1))

        best_next_action = np.max(q_table[next_q_value])
        q_table[q_value, action_index] += alpha * (
            reward + gamma * best_next_action - q_table[q_value, action_index]
        )
    except Exception as e:
        print(f"Q-테이블 갱신 중 오류 발생: {e}")


def wait_for_simulation_completion(env, timeout=30):
    start_time = time.time()
    while env.simulationRunning():                  # NS-3가 실행 중인지 확인
        if time.time() - start_time > timeout:
            print("시간초과: NS-3가 제 시간에 완료되지 못했습니다.")
            break
        time.sleep(1)  # 1초 대기

def safe_reset(env, max_retries=3):
    for attempt in range(max_retries):
        wait_for_simulation_completion(env)
        state = env.reset()
        if state is not None:
            return state
        print(f"Reset attempt {attempt + 1} failed.")
    print("Failed to reset NS-3 environment after multiple attempts.")
    return None

# 강화학습 에이전트 실행
for episode in range(episodes):
    state = env.reset()                 # 초기 상태
    print(f"초기 상태: {state}")    # 상태 출력

    if state is None or not isinstance(state, (list, np.ndarray)):
        print("상태 초기화 오류, 에피소드를 건너뜁니다.")
        print(f"상태 세부사항: {state}")
        env = ns3env.Ns3Env(port=port, startSim=True, simSeed=seed)
        continue

    total_reward = 0.0

    while True:
        action = choose_action(state, epsilon)  # 행동 선택
        next_state, reward, done, _ = env.step(action)  # 환경과 상호작용
        
        if next_state is None or not isinstance(next_state, (list, np.ndarray)):
            print(f"Next state is invalid: {next_state}, skipping step.")
            break
        total_reward += reward
        print(f'토탈 리워드 값: {total_reward}')
        # Q-테이블 업데이트
        update_q_table(state, action, reward, next_state)
        # 상태 업데이트
        state = next_state

        if done:
            break

    epsilon = max(epsilon_min, epsilon * epsilon_decay)     # 탐험률 감소

    print(f"시행횟수: {episode + 1}/{episodes} \t 보상 합계: {total_reward:.2f} \t 탐험률: {epsilon:.2f}")
    time.sleep(2)  # 시뮬레이션 종료 대기
env.close()
