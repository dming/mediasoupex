export type RtmpMsg ={
    messageType: number;
    timestamp: number;
    size: number;
    payload: Buffer;
};

export interface IRtmpConsumer {
    enqueue(msg: RtmpMsg): void;
}