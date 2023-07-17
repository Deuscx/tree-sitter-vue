import { describe, expect, it } from 'vitest'
import { one } from '../src/index'

describe('export ', () => {
  it('test number eql', () => {
    expect(one).eql(1)
  })
})
