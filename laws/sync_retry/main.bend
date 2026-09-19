import Base

type Result is Data:
  Success{}
  InvalidUrl{}
  NoAccount{}
  PayloadFailed{}
  ChallengeFailed{}
  SignFailed{}
  RequestFailed{}
  AuthFailed{}
  Unknown{}

type Attempt is Data:
  Initial{}
  First{}
  Second{}
  Third{}
  Capped{}

type Decision is Data:
  Decision{attempt: U32, delay: U32, needs_action: U32}

def number(attempt: Attempt) -> U32:
  match attempt:
    case Initial{}:
      0
    case First{}:
      1
    case Second{}:
      2
    case Third{}:
      3
    case Capped{}:
      4

def advance(attempt: Attempt) -> Attempt:
  match attempt:
    case Initial{}:
      First{}
    case First{}:
      Second{}
    case Second{}:
      Third{}
    case Third{}:
      Capped{}
    case Capped{}:
      Capped{}

def delay(attempt: Attempt) -> U32:
  match attempt:
    case Initial{}:
      5
    case First{}:
      5
    case Second{}:
      15
    case Third{}:
      30
    case Capped{}:
      60

def retry(attempt: Attempt) -> Decision:
  +next = advance(attempt)
  Decision{number(next), delay(next), 0}

def stop(attempt: Attempt) -> Decision:
  Decision{number(attempt), 0, 1}

def decide(result: Result, attempt: Attempt) -> Decision:
  match result:
    case Success{}:
      Decision{0, 0, 0}
    case InvalidUrl{}:
      stop(attempt)
    case NoAccount{}:
      stop(attempt)
    case PayloadFailed{}:
      stop(attempt)
    case ChallengeFailed{}:
      retry(attempt)
    case SignFailed{}:
      stop(attempt)
    case RequestFailed{}:
      retry(attempt)
    case AuthFailed{}:
      stop(attempt)
    case Unknown{}:
      stop(attempt)
