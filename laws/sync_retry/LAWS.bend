import Base
import ./main.bend as Policy

# The contract owns the schedule; the implementation must prove it agrees.
def schedule(attempt: Policy.Attempt) -> Policy.Decision:
  match attempt:
    case Policy.Initial{}:
      Policy.Decision{1, 5, 0}
    case Policy.First{}:
      Policy.Decision{2, 15, 0}
    case Policy.Second{}:
      Policy.Decision{3, 30, 0}
    case Policy.Third{}:
      Policy.Decision{4, 60, 0}
    case Policy.Capped{}:
      Policy.Decision{4, 60, 0}

def saved_delay(attempt: Policy.Attempt) -> U32:
  match attempt:
    case Policy.Initial{}:
      5
    case Policy.First{}:
      5
    case Policy.Second{}:
      15
    case Policy.Third{}:
      30
    case Policy.Capped{}:
      60

def saved_attempt(attempt: Policy.Attempt) -> U32:
  match attempt:
    case Policy.Initial{}:
      0
    case Policy.First{}:
      1
    case Policy.Second{}:
      2
    case Policy.Third{}:
      3
    case Policy.Capped{}:
      4

law success_resets:
  for attempt: Policy.Attempt
  {Policy.decide(Policy.Success{}, attempt) == Policy.Decision{0, 0, 0} : Policy.Decision}

law challenge_retries:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.ChallengeFailed{}, attempt) == schedule(attempt) : Policy.Decision}

law request_retries:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.RequestFailed{}, attempt) == schedule(attempt) : Policy.Decision}

law invalid_url_stops:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.InvalidUrl{}, attempt) == Policy.Decision{Policy.number(attempt), 0, 1} : Policy.Decision}

law no_account_stops:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.NoAccount{}, attempt) == Policy.Decision{Policy.number(attempt), 0, 1} : Policy.Decision}

law payload_failure_stops:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.PayloadFailed{}, attempt) == Policy.Decision{Policy.number(attempt), 0, 1} : Policy.Decision}

law signing_failure_stops:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.SignFailed{}, attempt) == Policy.Decision{Policy.number(attempt), 0, 1} : Policy.Decision}

law authentication_failure_stops:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.AuthFailed{}, attempt) == Policy.Decision{Policy.number(attempt), 0, 1} : Policy.Decision}

law unknown_result_stops:
  for +attempt: Policy.Attempt
  {Policy.decide(Policy.Unknown{}, attempt) == Policy.Decision{Policy.number(attempt), 0, 1} : Policy.Decision}

law saved_delay_matches:
  for +attempt: Policy.Attempt
  {Policy.delay(attempt) == saved_delay(attempt) : U32}

law attempt_number_matches:
  for +attempt: Policy.Attempt
  {Policy.number(attempt) == saved_attempt(attempt) : U32}
